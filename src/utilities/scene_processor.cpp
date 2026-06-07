#include "utilities/scene_processor.h"

#include <algorithm>
#include <cstdint>
#include <fastgltf/types.hpp>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <iterator>
#include <limits>
#include <set>
#include <span>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "models/aabb.h"
#include "models/primitive.h"
#include "renderer/buffers.h"
#include "renderer/cascade_shadow.h"
#include "renderer/renderer_types.h"
#include "renderer/shader/graphics_shader.h"
#include "renderer/textures.h"
#include "scene/handle.h"
#include "scene/scene.h"
#include "utilities/buffer_builder.h"
#include "utilities/texture_manager.h"
#include "utilities/transform.h"

namespace
{
AABB getSceneBounds(Scene &scene)
{
    glm::vec3 min{std::numeric_limits<float>::infinity()};
    glm::vec3 max{-std::numeric_limits<float>::infinity()};

    for (Scene::Entry &entry : scene.getAllSceneEntries())
    {
        using enum ShaderType;

        if (entry.shaderType == ocean || entry.shaderType == skybox)
            continue;

        for (Transform &transform : entry.transforms)
        {
            const AABB worldAABB{entry.aabb.transform(transform.getModelMatrix())};

            const glm::vec3 aabbMin{worldAABB.getMin()};
            const glm::vec3 aabbMax{worldAABB.getMax()};

            min = glm::min(min, aabbMin);
            max = glm::max(max, aabbMax);
        }
    }

    return {min, max};
}
} // namespace

SceneProcessor::SceneProcessor(Scene &scene, TextureManager &textureManager, bool shadowsEnabled)
    : shadows_{shadowsEnabled}, sceneAABB_{getSceneBounds(scene)}, totals_{getSceneTotals(scene)},
      bucketEntries_{createDrawEntries(scene)}, textureManager_{textureManager}
{
}

std::size_t SceneProcessor::getNumDynamicCubemaps() const
{
    return totals_.dynamicCubemaps;
}

SceneProcessor::ProcessedScene SceneProcessor::processScene()
{
    if (!textureManager_.allTexturesBuilt())
    {
        std::cerr << "ERROR: SceneProcessor::processScene() called before TextureManager::buildAllTextures()"
                  << std::endl;
        throw std::logic_error("SceneProcessor::processScene() called before TextureManager::buildAllTextures()");
    }

    // Staging
    auto VAO = createVAO(bucketEntries_);
    auto matricesMap = createMatricesBaseIndexMap(bucketEntries_);
    auto commands = createDrawCommands(bucketEntries_);
    auto buckets = createDrawBuckets(bucketEntries_);
    auto AABBs = createAABBs(bucketEntries_);
    auto vertexMap = createVertexBufferMap(bucketEntries_);
    auto matrices = createMatrices(bucketEntries_);
    auto metadata = createMetadata(bucketEntries_);

    auto drawBucketHandles = createDrawBucketHandles(bucketEntries_, buckets);
    auto visibilityMap = createVisibilityBufferMap(bucketEntries_, metadata, buckets, matricesMap);

    // GPU data upload
    auto bufferSizes = calculateBufferSizes(commands, buckets);
    auto SSBOs = createSSBOs(commands, matrices, metadata, visibilityMap, vertexMap, AABBs, bufferSizes);

    // Only store this so the metadata cubemaps can be updated later
    metadata_ = std::move(metadata);

    return {.VAOs = std::move(VAO),
            .SSBOs = std::move(SSBOs),
            .sceneDescriptor =
                {
                    .worldAABB = std::move(sceneAABB_),
                    .numInstances = totals_.instances,
                    .numVertices = totals_.vertices,
                    .numPrimitives = totals_.primitives,
                    .numParticles = totals_.particles,
                },
            .bufferDescriptor =
                {
                    .drawCommandsSize = bufferSizes.drawCommandsSize,
                    .cullResultsSize = bufferSizes.cullResults,
                    .drawCountsSize = bufferSizes.drawCounts,
                    .visibleInstancesSize = bufferSizes.visibleInstances,
                    .matricesBaseIndexMap = std::move(matricesMap),
                    .drawBucketHandles = std::move(drawBucketHandles),
                },
            .drawBuckets = std::move(buckets)};
}

SceneProcessor::SceneTotals SceneProcessor::getSceneTotals(Scene &scene)
{
    SceneTotals totals;

    for (Scene::Entry &entry : scene.getAllSceneEntries())
    {
        std::size_t numTransforms{entry.transforms.size()};
        std::size_t numPrimitives{entry.primitives.size()};

        if (entry.shaderType == ShaderType::water)
            ++totals.dynamicCubemaps;

        totals.matrices += numTransforms;
        totals.primitives += numPrimitives;
        totals.instances += numTransforms * numPrimitives;

        for (Primitive &primitive : entry.primitives)
        {
            totals.indices += primitive.indices.size();
            totals.vertices += primitive.vertices.size();

            if (entry.shaderType == ShaderType::particles)
                totals.particles += primitive.vertices.size();
        }
    }

    return totals;
}

// Build up all scene entries by shader type
std::vector<SceneProcessor::BucketEntry> SceneProcessor::createDrawEntries(Scene &scene)
{
    std::vector<BucketEntry> drawEntries;
    drawEntries.reserve(totals_.primitives);

    for (Scene::Entry &entry : scene.getAllSceneEntries())
    {
        for (Primitive &primitive : entry.primitives)
        {
            using enum fastgltf::AlphaMode;

            drawEntries.emplace_back(BucketEntry{.handle = entry.handle,
                                                 .primitive = &primitive,
                                                 .transforms = entry.transforms,
                                                 .shaderType = entry.shaderType,
                                                 .alphaMode = primitive.alphaMode});
        }
    }

    std::ranges::sort(drawEntries, [](const BucketEntry &a, const BucketEntry &b) {
        return std::tie(a.shaderType, a.alphaMode) < std::tie(b.shaderType, b.alphaMode);
    });

    return drawEntries;
}

BufferBuilder::Sizes SceneProcessor::calculateBufferSizes(const std::vector<DrawElementsIndirectCommand> &commands,
                                                          const std::vector<MultiDrawBucket> &buckets)
{
    auto alignSSBO = [](const std::size_t buffer, const GLint alignment) {
        std::size_t align{static_cast<std::size_t>(alignment)};

        return ((buffer + align - 1) / align) * align;
    };

    GLint alignment{0};
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &alignment);

    std::size_t drawCounts{buckets.size() * sizeof(std::uint32_t)};
    std::size_t cullResults{commands.size() * sizeof(std::uint32_t)};
    std::size_t visibleInstances{totals_.instances * sizeof(std::uint32_t)};
    std::size_t drawCommands{commands.size() * sizeof(DrawElementsIndirectCommand)};

    return {
        .metadata = commands.size() * sizeof(PrimitiveMetadata),
        .matrices = totals_.matrices * sizeof(glm::mat4),
        .bufferMap = totals_.instances * sizeof(VisibilityBufferMap),
        .drawCounts = shadows_ ? alignSSBO(drawCounts, alignment) : drawCounts,
        .cullResults = shadows_ ? alignSSBO(cullResults, alignment) : cullResults,
        .visibleInstances = shadows_ ? alignSSBO(visibleInstances, alignment) : visibleInstances,
        .drawCommandsSize = shadows_ ? alignSSBO(drawCommands, alignment) : drawCommands,
    };
}

std::vector<AABB> SceneProcessor::createAABBs(const std::vector<BucketEntry> &entries)
{
    std::vector<AABB> AABBs;
    AABBs.reserve(totals_.primitives);

    for (const BucketEntry &entry : entries)
    {
        const AABB &aabb{entry.primitive->aabb};
        AABBs.emplace_back(aabb);
    }

    return AABBs;
}

std::vector<std::set<Handle>> SceneProcessor::createDrawBucketHandles(const std::vector<BucketEntry> &entries,
                                                                      const std::vector<MultiDrawBucket> &drawBuckets)
{
    std::vector<std::set<Handle>> handles;
    handles.resize(drawBuckets.size());

    for (std::size_t i{0}; i < drawBuckets.size(); ++i)
    {
        std::uint32_t start{drawBuckets[i].drawCommandsBaseIndex};
        std::uint32_t end{drawBuckets[i].drawCommandsBaseIndex + drawBuckets[i].maxDrawCalls};

        for (std::size_t j{start}; j < end; ++j)
            handles[i].emplace(entries[j].handle);
    }

    return handles;
}

std::vector<DrawElementsIndirectCommand> SceneProcessor::createDrawCommands(const std::vector<BucketEntry> &entries)
{
    std::vector<DrawElementsIndirectCommand> commands;
    commands.reserve(totals_.primitives);

    std::size_t vaoIndex{0};
    std::size_t eboIndex{0};

    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        const BucketEntry &entry{entries[i]};
        const Primitive &primitive{*entry.primitive};
        const std::size_t numIndices{primitive.indices.size()};

        commands.emplace_back(
            DrawElementsIndirectCommand{.count = static_cast<unsigned int>(numIndices),
                                        .instanceCount = static_cast<unsigned int>(entry.transforms.size()),
                                        .firstIndex = static_cast<unsigned int>(eboIndex),
                                        .baseVertex = static_cast<int>(vaoIndex),
                                        .baseInstance = static_cast<unsigned int>(i)});

        vaoIndex += primitive.vertices.size();
        eboIndex += numIndices;
    }

    return commands;
}

std::vector<MultiDrawBucket> SceneProcessor::createDrawBuckets(const std::vector<BucketEntry> &entries)
{
    using enum ShaderType;
    using enum fastgltf::AlphaMode;

    std::vector<MultiDrawBucket> buckets;

    ShaderType currentShaderType{entries.front().shaderType};
    fastgltf::AlphaMode currentAlphaMode{entries.front().alphaMode};

    std::uint32_t start{0};
    std::uint32_t end{0};

    for (const BucketEntry &entry : entries)
    {
        if (currentShaderType != entry.shaderType || currentAlphaMode != entry.alphaMode)
        {
            buckets.emplace_back(MultiDrawBucket{.maxDrawCalls = end - start,
                                                 .drawCommandsBaseIndex = start,
                                                 .shaderType = currentShaderType,
                                                 .alphaMode = currentAlphaMode});

            start = end;
            currentShaderType = entry.shaderType;
            currentAlphaMode = entry.alphaMode;
        }

        ++end;
    }

    if (end - start > 0)
        buckets.emplace_back(MultiDrawBucket{
            .maxDrawCalls = end - start,
            .drawCommandsBaseIndex = start,
            .shaderType = currentShaderType,
            .alphaMode = currentAlphaMode,
        });

    return buckets;
}

std::unordered_map<Handle, std::uint32_t> SceneProcessor::createMatricesBaseIndexMap(
    const std::vector<BucketEntry> &entries)
{
    std::uint32_t matricesIndex{0};
    std::unordered_map<Handle, std::uint32_t> baseIndices;

    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        auto [iter, inserted] = baseIndices.try_emplace(entries[i].handle);

        if (inserted)
        {
            iter->second = matricesIndex;
            matricesIndex += static_cast<std::uint32_t>(entries[i].transforms.size());
        }
    }

    return baseIndices;
}

VAOs SceneProcessor::createVAO(const std::vector<BucketEntry> &entries)
{
    std::vector<Primitive::Vertex> vertices;
    std::vector<std::uint32_t> indices;

    vertices.reserve(totals_.vertices);
    indices.reserve(totals_.indices);

    for (const BucketEntry &entry : entries)
    {
        std::copy(entry.primitive->vertices.begin(), entry.primitive->vertices.end(), std::back_inserter(vertices));
        std::copy(entry.primitive->indices.begin(), entry.primitive->indices.end(), std::back_inserter(indices));
    }

    BufferBuilder builder;
    return builder.buildVAO(vertices, indices);
}

SSBOs SceneProcessor::createSSBOs(const std::vector<DrawElementsIndirectCommand> &commands,
                                  const std::vector<glm::mat4> &matrices,
                                  const std::vector<PrimitiveMetadata> &metadata,
                                  const std::vector<VisibilityBufferMap> &visibilityMap,
                                  const std::vector<VertexBufferMap> &vertexBufferMap, const std::vector<AABB> &aabbs,
                                  BufferBuilder::Sizes alignedSizes)
{
    BufferBuilder builder;

    std::size_t cascadeSized{static_cast<std::size_t>(shadows_ ? shadowCascadeCount + 1 : 1)};

    BufferBuilder::Sizes sizes{
        .metadata = alignedSizes.metadata,
        .matrices = alignedSizes.matrices,
        .bufferMap = alignedSizes.bufferMap,
        .drawCounts = alignedSizes.drawCounts * cascadeSized,
        .cullResults = alignedSizes.cullResults * cascadeSized,
        .visibleInstances = alignedSizes.visibleInstances * cascadeSized,
        .drawCommandsSize = alignedSizes.drawCommandsSize * cascadeSized,
    };

    return builder.buildSSBOs(commands, matrices, metadata, visibilityMap, vertexBufferMap, aabbs, sizes);
}

std::vector<glm::mat4> SceneProcessor::createMatrices(const std::vector<BucketEntry> &entries)
{
    std::vector<glm::mat4> matrices;
    matrices.reserve(totals_.matrices);

    std::unordered_set<Handle> seenHandles;

    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        auto [iter, inserted] = seenHandles.emplace(entries[i].handle);

        if (inserted)
        {
            for (Transform &transform : entries[i].transforms)
                matrices.emplace_back(transform.getModelMatrix());
        }
    }

    return matrices;
}

std::vector<PrimitiveMetadata> SceneProcessor::createMetadata(const std::vector<BucketEntry> &entries)
{
    std::vector<PrimitiveMetadata> metadata;
    metadata.resize(entries.size());
    setVisibleInstancesBaseIndex(entries, metadata);
    setMetadataMaterials(entries, metadata);

    return metadata;
}

void SceneProcessor::setVisibleInstancesBaseIndex(const std::vector<BucketEntry> &entries,
                                                  std::vector<PrimitiveMetadata> &metadata)
{
    std::uint32_t visibleIndex{0};

    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        const std::size_t numTransforms{entries[i].transforms.size()};

        metadata[i].visibleInstancesBaseIndex = visibleIndex;
        visibleIndex += static_cast<std::uint32_t>(numTransforms);
    }
}

void SceneProcessor::setMetadataMaterials(const std::vector<BucketEntry> &entries,
                                          std::vector<PrimitiveMetadata> &metadata)
{
    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        const BucketEntry &entry{entries[i]};
        PrimitiveMetadata &metaEntry{metadata[i]};

        const std::size_t matIndex{entry.primitive->materialIndex};
        std::span<const Material> materials{textureManager_.getMaterials(entry.handle)};

        if (matIndex >= materials.size())
        {
            std::cerr << "Material index out of bound for handle " << entry.handle << std::endl;
            throw std::runtime_error("Material index out of bounds");
        }

        const Material &material{materials[matIndex]};
        const SamplerArrayIndex &albedo{material.layers[Material::albedo]};
        const SamplerArrayIndex &ARM{material.layers[Material::ARM]};
        const SamplerArrayIndex &normal{material.layers[Material::normal]};
        const SamplerArrayIndex &cubemap{material.layers[Material::cubemap]};

        metaEntry.albedoArray = albedo.array;
        metaEntry.albedoLayer = albedo.layer;

        metaEntry.armArray = ARM.array;
        metaEntry.armLayer = ARM.layer;

        metaEntry.normalArray = normal.array;
        metaEntry.normalLayer = normal.layer;

        metaEntry.cubemapArray = cubemap.array;
        metaEntry.cubemapLayer = cubemap.layer;
    }
}

std::vector<VisibilityBufferMap> SceneProcessor::createVisibilityBufferMap(
    const std::vector<SceneProcessor::BucketEntry> &entries, const std::vector<PrimitiveMetadata> &metadata,
    const std::vector<MultiDrawBucket> &buckets, const std::unordered_map<Handle, std::uint32_t> matricesMap)
{
    std::vector<VisibilityBufferMap> map;
    map.reserve(totals_.instances);

    for (std::size_t i{0}; i < buckets.size(); ++i)
    {
        const MultiDrawBucket &bucket{buckets[i]};

        for (std::uint32_t j{bucket.drawCommandsBaseIndex}; j < bucket.drawCommandsBaseIndex + bucket.maxDrawCalls; ++j)
        {
            const Handle handle{entries[j].handle};
            const PrimitiveMetadata &metaEntry{metadata[j]};
            const std::span<Transform> transforms{entries[j].transforms};

            const std::uint32_t matricesBufferBaseIndex{matricesMap.at(handle)};
            const std::uint32_t visibleInstancesBufferBaseIndex{metaEntry.visibleInstancesBaseIndex};
            const std::uint32_t currentBucket{static_cast<std::uint32_t>(i)};

            for (std::size_t k{0}; k < transforms.size(); ++k)
            {
                map.emplace_back(VisibilityBufferMap{
                    .matricesIndex = matricesBufferBaseIndex + static_cast<std::uint32_t>(k),
                    .sharedIndex = j,
                    .drawBucket = currentBucket,
                    .drawCommandsBaseIndex = bucket.drawCommandsBaseIndex,
                    .visibleInstancesBaseIndex = visibleInstancesBufferBaseIndex,
                });
            }
        }
    }

    return map;
}

std::vector<VertexBufferMap> SceneProcessor::createVertexBufferMap(const std::vector<BucketEntry> &entries)
{
    std::size_t currentVertex{0};
    std::vector<VertexBufferMap> vertexBufferMap;

    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        using enum ShaderType;

        ShaderType type{entries[i].shaderType};

        if (type == particles)
        {
            for (std::size_t j{0}; j < entries[i].primitive->vertices.size(); ++j)
            {
                std::uint32_t vboIndex{static_cast<std::uint32_t>(currentVertex + j)};
                std::uint32_t sharedIndex{static_cast<std::uint32_t>(i)};

                vertexBufferMap.emplace_back(VertexBufferMap{.vboIndex = vboIndex, .sharedIndex = sharedIndex});
            }
        }

        currentVertex += entries[i].primitive->vertices.size();
    }

    return vertexBufferMap;
}

void SceneProcessor::updateMetadataCubemaps(GLuint metadataBufferID)
{
    for (std::size_t i{0}; i < bucketEntries_.size(); ++i)
    {
        using enum ShaderType;

        if (bucketEntries_[i].shaderType != water && bucketEntries_[i].shaderType != ocean)
            continue;

        PrimitiveMetadata &metaEntry{metadata_[i]};

        const Handle handle{bucketEntries_[i].handle};
        const Primitive &primitive{*bucketEntries_[i].primitive};
        std::span<const Material> materials{textureManager_.getMaterials(handle)};
        const Material &material{materials[primitive.materialIndex]};

        const SamplerArrayIndex &cubemap{material.layers[Material::cubemap]};
        metaEntry.cubemapArray = cubemap.array;
        metaEntry.cubemapLayer = cubemap.layer;

        glNamedBufferSubData(metadataBufferID, static_cast<GLintptr>(i * sizeof(PrimitiveMetadata)),
                             static_cast<GLsizeiptr>(sizeof(PrimitiveMetadata)), &metaEntry);
    }
}
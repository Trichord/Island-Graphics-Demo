#include "scene_processor.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "aabb.h"
#include "buffer_builder.h"
#include "buffers.h"
#include "fastgltf/types.hpp"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "graphics_shader.h"
#include "handle.h"
#include "primitive.h"
#include "renderer_types.h"
#include "scene.h"
#include "texture_manager.h"
#include "textures.h"
#include "transform.h"

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

SceneProcessor::SceneProcessor(Scene &scene, TextureManager &textureManager)
    : sceneAABB_{getSceneBounds(scene)}, totals_{getSceneTotals(scene)}, drawEntries_{createDrawEntries(scene)},
      textureManager_{textureManager}
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
    auto VAO = createVAO(drawEntries_);
    auto matricesMap = createMatricesBaseIndexMap(drawEntries_);
    auto shaderBucketMap = createShaderBucketMap();
    auto commands = createDrawCommands(drawEntries_);
    auto buckets = createDrawBuckets(drawEntries_);
    auto bufferSizes = calculateBufferSizes(commands, buckets);
    auto AABBs = createAABBs(drawEntries_);
    auto SSBOs = createSSBOs(commands, AABBs, bufferSizes);

    // GPU data upload
    auto matrices = createMatrices(drawEntries_);
    glNamedBufferSubData(SSBOs.matrices, 0, static_cast<GLsizeiptr>(matrices.size() * sizeof(glm::mat4)),
                         matrices.data());

    auto metadata = createMetadata(drawEntries_);
    glNamedBufferSubData(SSBOs.metadata, 0, static_cast<GLsizeiptr>(metadata.size() * sizeof(PrimitiveMetadata)),
                         metadata.data());

    auto map = createBufferMap(drawEntries_, metadata, buckets, matricesMap);
    glNamedBufferSubData(SSBOs.map, 0, static_cast<GLsizeiptr>(map.size() * sizeof(BufferMap)), map.data());

    // Only store this so the metadata cubemaps can be updated later
    metadata_ = std::move(metadata);

    return {
        .ssbos = std::move(SSBOs),
        .vao = std::move(VAO),
        .sceneData =
            {
                .worldAABB = std::move(sceneAABB_),
                .numInstances = totals_.instances,
            },
        .bufferData =
            {
                .buckets = std::move(buckets),
                .matricesBaseIndexMap = std::move(matricesMap),
            },
        .bucketMap = std::move(shaderBucketMap),
        .commands = std::move(commands),
    };
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
        }
    }

    return totals;
}

// Build up all scene entries by shader type
std::vector<SceneProcessor::DrawEntry> SceneProcessor::createDrawEntries(Scene &scene)
{
    std::vector<DrawEntry> drawEntries;
    drawEntries.reserve(totals_.primitives);

    for (Scene::Entry &entry : scene.getAllSceneEntries())
    {
        for (Primitive &primitive : entry.primitives)
        {
            using enum fastgltf::AlphaMode;

            drawEntries.emplace_back(DrawEntry{.handle = entry.handle,
                                               .primitive = &primitive,
                                               .transforms = entry.transforms,
                                               .shaderType = entry.shaderType,
                                               .alphaMode = primitive.alphaMode});
        }
    }

    std::ranges::sort(drawEntries, [](const DrawEntry &a, const DrawEntry &b) {
        return std::tie(a.shaderType, a.alphaMode) < std::tie(b.shaderType, b.alphaMode);
    });

    return drawEntries;
}

BufferBuilder::Sizes SceneProcessor::calculateBufferSizes(const std::vector<DrawElementsIndirectCommand> &commands,
                                                          const std::vector<MultiDrawBucket> &buckets)
{
    BufferBuilder::Sizes bufferSizes;

    bufferSizes.metadata = commands.size() * sizeof(PrimitiveMetadata);
    bufferSizes.matrices = totals_.matrices * sizeof(glm::mat4);
    bufferSizes.bufferMap = totals_.instances * sizeof(BufferMap);
    bufferSizes.drawCounts = buckets.size() * sizeof(std::uint32_t);
    bufferSizes.cullResults = commands.size() * sizeof(std::uint32_t);
    bufferSizes.visibleInstances = totals_.instances * sizeof(std::uint32_t);

    return bufferSizes;
}

std::vector<AABB> SceneProcessor::createAABBs(const std::vector<DrawEntry> &entries)
{
    std::vector<AABB> AABBs;
    AABBs.reserve(totals_.primitives);

    for (const DrawEntry &entry : entries)
    {
        const AABB &aabb{entry.primitive->aabb};
        AABBs.emplace_back(aabb);
    }

    return AABBs;
}

std::unordered_map<ShaderType, SceneProcessor::BucketData> SceneProcessor::createShaderBucketMap()
{
    std::unordered_map<ShaderType, BucketData> bucketMap;

    for (std::size_t i{0}; i < drawEntries_.size(); ++i)
    {
        DrawEntry &entry{drawEntries_[i]};

        auto [iter, inserted] = bucketMap.try_emplace(entry.shaderType);
        auto &bucketData = iter->second;

        if (inserted)
        {
            bucketData.startIndex = i;
            bucketData.endIndex = i;
        }
        else
        {
            bucketData.endIndex = i;
        }

        bucketData.handles.emplace(entry.handle);
        bucketData.vertexCount.emplace_back(entry.primitive->vertices.size());
    }

    return bucketMap;
}

std::vector<DrawElementsIndirectCommand> SceneProcessor::createDrawCommands(const std::vector<DrawEntry> &drawEntries)
{
    std::vector<DrawElementsIndirectCommand> commands;
    commands.reserve(totals_.primitives);

    std::size_t vaoIndex{0};
    std::size_t eboIndex{0};

    for (std::size_t i{0}; i < drawEntries.size(); ++i)
    {
        const DrawEntry &entry{drawEntries[i]};
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

std::vector<MultiDrawBucket> SceneProcessor::createDrawBuckets(const std::vector<DrawEntry> &entries)
{
    using enum ShaderType;
    using enum fastgltf::AlphaMode;

    std::vector<MultiDrawBucket> buckets;

    ShaderType currentShaderType{entries.front().shaderType};
    fastgltf::AlphaMode currentAlphaMode{entries.front().alphaMode};

    std::uint32_t start{0};
    std::uint32_t end{0};

    for (const DrawEntry &entry : entries)
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
    const std::vector<DrawEntry> &entries)
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

GlobalVAO SceneProcessor::createVAO(const std::vector<DrawEntry> &entries)
{
    std::vector<Primitive::Vertex> vertices;
    std::vector<std::uint32_t> indices;

    vertices.reserve(totals_.vertices);
    indices.reserve(totals_.indices);

    for (const DrawEntry &entry : entries)
    {
        std::copy(entry.primitive->vertices.begin(), entry.primitive->vertices.end(), std::back_inserter(vertices));
        std::copy(entry.primitive->indices.begin(), entry.primitive->indices.end(), std::back_inserter(indices));
    }

    BufferBuilder builder;
    return builder.buildVAO(vertices, indices);
}

SSBOs SceneProcessor::createSSBOs(const std::vector<DrawElementsIndirectCommand> &commands,
                                  const std::vector<AABB> &AABBs, const BufferBuilder::Sizes bufferSizes)
{
    BufferBuilder builder;
    return builder.buildSSBOs(commands, AABBs, bufferSizes);
}

std::vector<glm::mat4> SceneProcessor::createMatrices(const std::vector<DrawEntry> &entries)
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

std::vector<SceneProcessor::PrimitiveMetadata> SceneProcessor::createMetadata(const std::vector<DrawEntry> &entries)
{
    std::vector<PrimitiveMetadata> metadata;
    metadata.resize(entries.size());
    setVisibleInstancesBaseIndex(entries, metadata);
    setMetadataMaterials(entries, metadata);

    return metadata;
}

void SceneProcessor::setVisibleInstancesBaseIndex(const std::vector<DrawEntry> &entries,
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

void SceneProcessor::setMetadataMaterials(const std::vector<DrawEntry> &entries,
                                          std::vector<PrimitiveMetadata> &metadata)
{
    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        const DrawEntry &entry{entries[i]};
        PrimitiveMetadata &metaEntry{metadata[i]};

        const int matIndex{entry.primitive->materialIndex};
        std::span<const Material> materials{textureManager_.getMaterials(entry.handle)};

        if (matIndex < 0 || matIndex >= static_cast<int>(materials.size()))
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

std::vector<SceneProcessor::BufferMap> SceneProcessor::createBufferMap(
    const std::vector<SceneProcessor::DrawEntry> &entries,
    const std::vector<SceneProcessor::PrimitiveMetadata> &metadata, const std::vector<MultiDrawBucket> &buckets,
    const std::unordered_map<Handle, std::uint32_t> matricesMap)
{
    std::vector<BufferMap> map;
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
                map.emplace_back(BufferMap{
                    .matricesIndex = matricesBufferBaseIndex + static_cast<std::uint32_t>(k),
                    .sharedBaseIndex = j,
                    .drawBucket = currentBucket,
                    .drawCommandsBaseIndex = bucket.drawCommandsBaseIndex,
                    .visibleInstancesBaseIndex = visibleInstancesBufferBaseIndex,
                });
            }
        }
    }

    return map;
}

void SceneProcessor::updateMetadataCubemaps(GLuint metadataBufferID)
{
    for (std::uint32_t i{0}; i < static_cast<std::uint32_t>(drawEntries_.size()); ++i)
    {
        using enum ShaderType;

        if (drawEntries_[i].shaderType != water && drawEntries_[i].shaderType != ocean)
            continue;

        PrimitiveMetadata &metaEntry{metadata_[i]};

        const Handle handle{drawEntries_[i].handle};
        const Primitive &primitive{*drawEntries_[i].primitive};
        std::span<const Material> materials{textureManager_.getMaterials(handle)};
        const Material &material{materials[primitive.materialIndex]};

        const SamplerArrayIndex &cubemap{material.layers[Material::cubemap]};
        metaEntry.cubemapArray = cubemap.array;
        metaEntry.cubemapLayer = cubemap.layer;

        glNamedBufferSubData(metadataBufferID, static_cast<GLintptr>(i * sizeof(PrimitiveMetadata)),
                             static_cast<GLsizeiptr>(sizeof(PrimitiveMetadata)), &metaEntry);
    }
}
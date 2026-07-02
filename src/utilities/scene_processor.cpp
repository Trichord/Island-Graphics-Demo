#include "utilities/scene_processor.h"

#include <algorithm>
#include <cstdint>
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

    for (const Scene::Entry &entry : scene.getAllSceneEntries())
    {
        using enum ShaderType;

        if (entry.shaderType == ocean || entry.shaderType == skybox)
            continue;

        for (const Primitive &primitive : entry.primitives)
        {
            for (const Transform &transform : entry.worldTransforms)
            {
                const AABB worldAABB{primitive.aabb.transform(transform.getModelMatrix())};
                const glm::vec3 aabbMin{worldAABB.getMin()};
                const glm::vec3 aabbMax{worldAABB.getMax()};

                min = glm::min(min, aabbMin);
                max = glm::max(max, aabbMax);
            }
        }
    }

    return {min, max};
}
} // namespace

SceneProcessor::SceneProcessor(Scene &scene, TextureManager &textureManager, bool shadowsEnabled)
    : shadows_{shadowsEnabled}, sceneAABB_{getSceneBounds(scene)}, totals_{getSceneTotals(scene)},
      bucketEntries_{createBucketEntries(scene, totals_)}, textureManager_{textureManager}
{
}

std::size_t SceneProcessor::getNumDynamicCubemaps() const
{
    return totals_.dynamicCubemaps;
}

SceneProcessor::ProcessedScene SceneProcessor::processScene() const
{
    if (!textureManager_.allTexturesBuilt())
    {
        std::cerr << "ERROR: SceneProcessor::processScene() called before TextureManager::buildAllTextures()"
                  << std::endl;
        throw std::logic_error("SceneProcessor::processScene() called before TextureManager::buildAllTextures()");
    }

    // Stage One (bucket entries only)
    auto metadataBridge = createMetadataBridge(bucketEntries_);
    auto matricesMap = createMatricesBaseIndexMap(bucketEntries_);
    auto commands = createDrawCommands(bucketEntries_);
    auto buckets = createDrawBuckets(bucketEntries_);
    auto AABBs = createAABBs(bucketEntries_);
    auto particleMap = createParticleMap(bucketEntries_);
    auto matrices = createMatrices(bucketEntries_);
    auto metadata = createMetadata(bucketEntries_);
    auto boneTransformMap = createBoneTransformMap(bucketEntries_);
    auto processedVertices = createProcessedVertices(bucketEntries_);
    auto particleBucketStart = getParticleBucketStart(bucketEntries_);

    // Stage two (multi-vector dependent)
    auto drawBucketHandles = createDrawBucketHandles(bucketEntries_, buckets);
    auto visibilityMap = createVisibilityBufferMap(bucketEntries_, metadata, buckets);
    auto vertexSkins = createVertexSkins(bucketEntries_, boneTransformMap);
    auto bufferSizes = calculateBufferSizes(commands, buckets);

    // GPU data upload
    auto VAO = createVAO(bucketEntries_, processedVertices.vertices);
    auto SSBOs = createSSBOs(commands, matrices, metadata, vertexSkins, processedVertices.skinnedVertices,
                             visibilityMap, particleMap, AABBs, bufferSizes);

    return {.VAOs = std::move(VAO),
            .SSBOs = std::move(SSBOs),
            .sceneDescriptors =
                {
                    .worldAABB = std::move(sceneAABB_),
                    .numInstances = totals_.instances,
                    .numVertices = totals_.vertices,
                    .numSkinnedVertices = totals_.skinnedVertices,
                    .numPrimitives = totals_.primitives,
                    .numParticles = totals_.particles,
                },
            .bufferDescriptors =
                {
                    .drawCommandsSize = bufferSizes.drawCommands,
                    .cullResultsSize = bufferSizes.cullResults,
                    .drawCountsSize = bufferSizes.drawCounts,
                    .visibleInstancesSize = bufferSizes.visibleInstances,
                    .particleBaseVBOIndex = particleBucketStart,
                    .metadataBridge = std::move(metadataBridge),
                    .drawBucketHandles = std::move(drawBucketHandles),
                    .matricesBaseIndexMap = std::move(matricesMap),
                    .boneTransformsBaseIndexMap = std::move(boneTransformMap),
                },
            .drawBuckets = std::move(buckets)};
}

SceneProcessor::SceneTotals SceneProcessor::getSceneTotals(Scene &scene) const
{
    SceneTotals totals;

    for (const Scene::Entry &entry : scene.getAllSceneEntries())
    {
        std::size_t numTransforms{entry.worldTransforms.size()};
        std::size_t numPrimitives{entry.primitives.size()};

        if (entry.shaderType == ShaderType::water)
            ++totals.dynamicCubemaps;

        totals.primitives += numPrimitives;
        totals.instances += numTransforms * numPrimitives;

        const bool hasAnimation{entry.entryAnimation.has_value()};
        const bool hasSkin{hasAnimation && entry.entryAnimation->skeletalAnimation.skeleton.skin.has_value()};

        for (const Primitive &primitive : entry.primitives)
        {
            totals.indices += primitive.indices.size();
            totals.vertices += primitive.vertices.size();

            if (entry.shaderType == ShaderType::particles)
                totals.particles += primitive.vertices.size();

            if (hasSkin)
                totals.skinnedVertices += primitive.vertices.size();
        }

        if (!hasSkin)
            continue;

        const Skeleton::Skin &skin = entry.entryAnimation->skeletalAnimation.skeleton.skin.value();
        totals.boneTransforms += skin.invBindMatrices.size();
    }

    return totals;
}

// Build up all scene entries by shader type
std::vector<SceneProcessor::BucketEntry> SceneProcessor::createBucketEntries(Scene &scene,
                                                                             const SceneTotals &totals) const
{
    std::vector<BucketEntry> bucketEntries;
    bucketEntries.reserve(totals.primitives);

    std::size_t originalIndex{0};
    for (const Scene::Entry &entry : scene.getAllSceneEntries())
    {
        for (const Primitive &primitive : entry.primitives)
        {
            using enum fastgltf::AlphaMode;

            bucketEntries.emplace_back(BucketEntry{.handle = entry.handle,
                                                   .primitive = &primitive,
                                                   .transforms = entry.worldTransforms,
                                                   .animation = entry.entryAnimation,
                                                   .shaderType = entry.shaderType,
                                                   .alphaMode = primitive.alphaMode,
                                                   .originalIndex = originalIndex++});
        }
    }

    std::ranges::sort(bucketEntries, [](const BucketEntry &a, const BucketEntry &b) {
        return std::tie(a.shaderType, a.alphaMode) < std::tie(b.shaderType, b.alphaMode);
    });

    return bucketEntries;
}

SceneProcessor::ProcessedVertices SceneProcessor::createProcessedVertices(const std::vector<BucketEntry> &entries) const
{
    ProcessedVertices processed;
    processed.vertices.resize(totals_.vertices);

    std::uint32_t currentVertex{0};

    for (const BucketEntry &entry : entries)
    {
        // Particles hijack the vertex, don't overwrite
        const bool update{entry.shaderType != ShaderType::particles};

        const bool hasSkin{entry.primitive->vertexSkins.has_value()};

        for (std::size_t i{0}; i < entry.primitive->vertices.size(); ++i)
        {
            const Primitive::Vertex &vertex{entry.primitive->vertices[i]};

            const Primitive::Vertex updatedVertex{.position = vertex.position,
                                                  .normal = vertex.normal,
                                                  .tangent = vertex.tangent,
                                                  .texCoords = vertex.texCoords,
                                                  .vboIndex = currentVertex,
                                                  .color = vertex.color};

            if (update)
                processed.vertices[currentVertex] = updatedVertex;
            else
                processed.vertices[currentVertex] = vertex;

            if (hasSkin)
                processed.skinnedVertices.emplace_back(updatedVertex);

            ++currentVertex;
        }
    }

    return processed;
}

BufferBuilder::Sizes SceneProcessor::calculateBufferSizes(const std::vector<DrawElementsIndirectCommand> &commands,
                                                          const std::vector<MultiDrawBucket> &buckets) const
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
        .matrices = totals_.instances * sizeof(glm::mat4),
        .bufferMap = totals_.instances * sizeof(VisibilityBufferMap),
        .drawCounts = shadows_ ? alignSSBO(drawCounts, alignment) : drawCounts,
        .cullResults = shadows_ ? alignSSBO(cullResults, alignment) : cullResults,
        .visibleInstances = shadows_ ? alignSSBO(visibleInstances, alignment) : visibleInstances,
        .drawCommands = shadows_ ? alignSSBO(drawCommands, alignment) : drawCommands,
        .boneTransforms = totals_.boneTransforms * sizeof(glm::mat4),
    };
}

std::vector<AABB> SceneProcessor::createAABBs(const std::vector<BucketEntry> &entries) const
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

std::vector<std::set<Handle>> SceneProcessor::createDrawBucketHandles(
    const std::vector<BucketEntry> &entries, const std::vector<MultiDrawBucket> &drawBuckets) const
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

std::vector<DrawElementsIndirectCommand> SceneProcessor::createDrawCommands(
    const std::vector<BucketEntry> &entries) const
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

std::vector<MultiDrawBucket> SceneProcessor::createDrawBuckets(const std::vector<BucketEntry> &entries) const
{
    using enum ShaderType;
    using enum AlphaMode;

    if (entries.empty())
        return {};

    std::vector<MultiDrawBucket> buckets;

    ShaderType currentShaderType{entries.front().shaderType};
    AlphaMode currentAlphaMode{entries.front().alphaMode};

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

std::vector<std::vector<std::uint32_t>> SceneProcessor::createMatricesBaseIndexMap(
    const std::vector<BucketEntry> &entries) const
{
    std::uint32_t matricesIndex{0};
    std::vector<std::vector<std::uint32_t>> baseIndices;

    for (const BucketEntry &entry : entries)
    {
        if (entry.handle >= baseIndices.size())
            baseIndices.resize(entry.handle + 1);

        baseIndices[entry.handle].emplace_back(matricesIndex);
        matricesIndex += static_cast<std::uint32_t>(entry.transforms.size());
    }

    return baseIndices;
}

VAOs SceneProcessor::createVAO(const std::vector<BucketEntry> &entries,
                               const std::vector<Primitive::Vertex> &vertices) const
{
    std::vector<std::uint32_t> indices;
    indices.reserve(totals_.indices);

    for (const BucketEntry &entry : entries)
        indices.insert(indices.end(), entry.primitive->indices.begin(), entry.primitive->indices.end());

    BufferBuilder builder;
    return builder.buildVAO(vertices, indices);
}

SSBOs SceneProcessor::createSSBOs(const std::vector<DrawElementsIndirectCommand> &commands,
                                  const std::vector<glm::mat4> &matrices,
                                  const std::vector<PrimitiveMetadata> &metadata,
                                  const std::vector<Primitive::VertexSkin> &vertexSkins,
                                  const std::vector<Primitive::Vertex> &originalSkinnedVerts,
                                  const std::vector<VisibilityBufferMap> &visibilityMap,
                                  const std::vector<std::uint32_t> &particleMap, const std::vector<AABB> &aabbs,
                                  BufferBuilder::Sizes alignedSizes) const
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
        .drawCommands = alignedSizes.drawCommands * cascadeSized,
        .boneTransforms = alignedSizes.boneTransforms,
    };

    return builder.buildSSBOs(commands, matrices, metadata, vertexSkins, originalSkinnedVerts, visibilityMap,
                              particleMap, aabbs, sizes);
}

std::vector<glm::mat4> SceneProcessor::createMatrices(const std::vector<BucketEntry> &entries) const
{
    std::vector<glm::mat4> matrices;
    matrices.reserve(totals_.instances);

    for (const BucketEntry &entry : entries)
        for (const Transform &transform : entry.transforms)
            matrices.emplace_back(transform.getModelMatrix() * entry.primitive->modelSpaceTransform);

    return matrices;
}

std::vector<PrimitiveMetadata> SceneProcessor::createMetadata(const std::vector<BucketEntry> &entries) const
{
    std::vector<PrimitiveMetadata> metadata;
    metadata.resize(entries.size());
    setVisibleInstancesBaseIndex(entries, metadata);
    setMetadataMaterials(entries, metadata);

    return metadata;
}

void SceneProcessor::setVisibleInstancesBaseIndex(const std::vector<BucketEntry> &entries,
                                                  std::vector<PrimitiveMetadata> &metadata) const
{
    std::uint32_t visibleIndex{0};

    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        const std::size_t numTransforms{entries[i].transforms.size()};

        metadata[i].visibleInstancesBaseIndex = visibleIndex;
        visibleIndex += static_cast<std::uint32_t>(numTransforms);
    }
}

std::size_t SceneProcessor::getParticleBucketStart(const std::vector<BucketEntry> &entries) const
{
    std::uint32_t vboIndex{0};

    for (const BucketEntry &entry : entries)
        if (entry.shaderType == ShaderType::particles)
            break;
        else
            vboIndex += entry.primitive->vertices.size();

    return vboIndex;
}

void SceneProcessor::setMetadataMaterials(const std::vector<BucketEntry> &entries,
                                          std::vector<PrimitiveMetadata> &metadata) const
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

        metaEntry.albedo = albedo;
        metaEntry.ARM = ARM;
        metaEntry.normal = normal;
        metaEntry.cubemap = cubemap;
    }
}

std::vector<VisibilityBufferMap> SceneProcessor::createVisibilityBufferMap(
    const std::vector<SceneProcessor::BucketEntry> &entries, const std::vector<PrimitiveMetadata> &metadata,
    const std::vector<MultiDrawBucket> &buckets) const
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
            std::span<const Transform> transforms{entries[j].transforms};

            const std::uint32_t visibleInstancesBufferBaseIndex{metaEntry.visibleInstancesBaseIndex};
            const std::uint32_t currentBucket{static_cast<std::uint32_t>(i)};

            for (const Transform &transform : transforms)
            {
                map.emplace_back(VisibilityBufferMap{
                    .sharedBufferIndex = j,
                    .drawBucket = currentBucket,
                    .drawCommandsBaseIndex = bucket.drawCommandsBaseIndex,
                    .visibleInstancesBaseIndex = visibleInstancesBufferBaseIndex,
                });
            }
        }
    }

    return map;
}

std::vector<std::uint32_t> SceneProcessor::createParticleMap(const std::vector<BucketEntry> &entries) const
{
    std::vector<std::uint32_t> particleMap;

    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        if (entries[i].shaderType != ShaderType::particles)
            continue;

        for (std::size_t j{0}; j < entries[i].primitive->vertices.size(); ++j)
            particleMap.emplace_back(static_cast<std::uint32_t>(i));
    }

    return particleMap;
}

std::unordered_map<Handle, std::uint32_t> SceneProcessor::createBoneTransformMap(
    const std::vector<BucketEntry> &entries) const
{
    std::unordered_map<Handle, std::uint32_t> boneTransformsMap;
    std::uint32_t boneBufferOffset{0};

    for (const BucketEntry &entry : entries)
    {
        if (!entry.primitive->vertexSkins.has_value())
            continue;

        // Per model - global transforms for bones (bone IDs)
        auto [iter, inserted] = boneTransformsMap.try_emplace(entry.handle, boneBufferOffset);

        if (inserted)
        {
            const auto &skin = entry.animation->skeletalAnimation.skeleton.skin;

            std::uint32_t numBoneTransforms{static_cast<std::uint32_t>(skin->invBindMatrices.size())};
            boneBufferOffset += numBoneTransforms;
        }
    }

    return boneTransformsMap;
}

std::vector<std::uint32_t> SceneProcessor::createMetadataBridge(const std::vector<BucketEntry> &entries) const
{
    std::vector<std::uint32_t> bridge(entries.size());

    for (std::size_t i{0}; i < entries.size(); ++i)
        bridge[entries[i].originalIndex] = static_cast<std::uint32_t>(i);

    return bridge;
}

std::vector<Primitive::VertexSkin> SceneProcessor::createVertexSkins(
    const std::vector<BucketEntry> &entries, const std::unordered_map<Handle, std::uint32_t> &boneTransformMap) const
{
    std::vector<Primitive::VertexSkin> skinData;

    for (const BucketEntry &entry : entries)
    {
        std::size_t numVertices{entry.primitive->vertices.size()};

        if (!entry.primitive->vertexSkins.has_value())
            continue;

        const std::vector<Primitive::VertexSkin> &vertexSkins{entry.primitive->vertexSkins.value()};

        auto iter = boneTransformMap.find(entry.handle);
        if (iter == boneTransformMap.end())
        {
            std::cerr << "ERROR: Handle " << entry.handle << " missing from bone transform map" << std::endl;
            throw std::runtime_error("Missing handle from bone transforms");
        }

        // Update each vertex's bone ID's to match the global offset in the SSBO (bone transform map gives base index)
        std::size_t bufferOffset{iter->second};

        for (std::size_t i{0}; i < numVertices; ++i)
        {
            glm::uvec4 boneIDs{vertexSkins[i].boneIDs};

            for (std::size_t j{0}; j < 4; ++j)
                if (vertexSkins[i].weights[j] > 0.0f)
                    boneIDs[j] += bufferOffset;

            skinData.push_back({vertexSkins[i].weights, boneIDs});
        }
    }

    return skinData;
}
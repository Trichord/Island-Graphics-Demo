#include "utilities/scene_processor.h"

#include <algorithm>
#include <cstdint>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <limits>
#include <set>
#include <span>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "models/aabb.h"
#include "models/animation/skeleton.h"
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
AABB getSceneBounds(const Scene &scene)
{
    glm::vec3 min{std::numeric_limits<float>::infinity()};
    glm::vec3 max{-std::numeric_limits<float>::infinity()};

    bool hasGeometry{false};

    for (const Scene::Entry &entry : scene.getAllSceneEntries())
    {
        using enum ShaderType;

        if (entry.shaderType == ocean || entry.shaderType == skybox)
            continue;

        hasGeometry = true;

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

    if (hasGeometry)
        return {min, max};

    // Ocean + skybox only combo would have infinite bounds. No shadow receivers
    return {glm::vec3{-1.0f}, glm::vec3{1.0f}};
}
} // namespace

SceneProcessor::ProcessedScene SceneProcessor::processScene(const Scene &scene, const TextureManager &textureManager,
                                                            bool shadowsEnabled)
{
    if (!textureManager.allTexturesBuilt())
    {
        std::cerr << "ERROR: SceneProcessor::processScene() called before TextureManager::buildAllTextures()"
                  << std::endl;
        throw std::logic_error("SceneProcessor::processScene() called before TextureManager::buildAllTextures()");
    }

    // Stage One - Scene dependent initialization
    auto totals = getSceneTotals(scene);
    auto sceneAABB = getSceneBounds(scene);
    auto bucketEntries = createBucketEntries(scene, totals);

    // Stage Two - bucket entries only
    auto buckets = createDrawBuckets(bucketEntries);
    auto metadataBridge = createMetadataBridge(bucketEntries);
    auto matricesMap = createMatricesBaseIndexMap(bucketEntries);
    auto boneTransformMap = createBoneTransformMap(bucketEntries);
    auto particleBucketStart = getParticleBucketStart(bucketEntries);

    // Stage Three - multiple dependencies
    auto AABBs = createAABBs(bucketEntries, totals);
    auto matrices = createMatrices(bucketEntries, totals);
    auto commands = createDrawCommands(bucketEntries, totals);
    auto particleMap = createParticleMap(bucketEntries, totals);
    auto metadata = createMetadata(textureManager, bucketEntries);
    auto processedVertices = createProcessedVertices(bucketEntries, totals);
    auto drawBucketHandles = createDrawBucketHandles(bucketEntries, buckets);
    auto vertexSkins = createVertexSkins(bucketEntries, boneTransformMap, totals);
    auto bufferSizes = calculateBufferSizes(commands, buckets, totals, shadowsEnabled);
    auto visibilityMap = createVisibilityBufferMap(bucketEntries, metadata, buckets, totals);

    // Stage Four - GPU data upload
    auto VAO = createVAO(bucketEntries, processedVertices.vertices, totals);
    auto SSBOs = createSSBOs(commands, matrices, metadata, vertexSkins, processedVertices.skinnedVertices,
                             visibilityMap, particleMap, AABBs, bufferSizes, shadowsEnabled);

    return {.VAOs = std::move(VAO),
            .SSBOs = std::move(SSBOs),
            .sceneDescriptors =
                {
                    .worldAABB = sceneAABB,
                    .numInstances = totals.instances,
                    .numVertices = totals.vertices,
                    .numSkinnedVertices = totals.skinnedVertices,
                    .numPrimitives = totals.primitives,
                    .numParticles = totals.particles,
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

SceneProcessor::SceneTotals SceneProcessor::getSceneTotals(const Scene &scene)
{
    SceneTotals totals;

    for (const Scene::Entry &entry : scene.getAllSceneEntries())
    {
        std::size_t numTransforms{entry.worldTransforms.size()};
        std::size_t numPrimitives{entry.primitives.size()};

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
std::vector<SceneProcessor::BucketEntry> SceneProcessor::createBucketEntries(const Scene &scene,
                                                                             const SceneTotals &totals)
{
    BucketEntries bucketEntries;
    bucketEntries.reserve(totals.primitives);

    std::size_t originalIndex{0};
    for (const Scene::Entry &entry : scene.getAllSceneEntries())
    {
        std::size_t primitiveIndex{0};

        for (const Primitive &primitive : entry.primitives)
        {
            bucketEntries.emplace_back(BucketEntry{.handle = entry.handle,
                                                   .primitive = &primitive,
                                                   .transforms = entry.worldTransforms,
                                                   .animation = entry.entryAnimation,
                                                   .shaderType = entry.shaderType,
                                                   .alphaMode = primitive.alphaMode,
                                                   .originalSceneIndex = originalIndex++,
                                                   .originalPrimitiveIndex = primitiveIndex++});
        }
    }

    std::ranges::sort(bucketEntries, [](const BucketEntry &a, const BucketEntry &b) {
        return std::tie(a.shaderType, a.alphaMode, a.originalSceneIndex) <
               std::tie(b.shaderType, b.alphaMode, b.originalSceneIndex);
    });

    return bucketEntries;
}

SceneProcessor::ProcessedVertices SceneProcessor::createProcessedVertices(const BucketEntries &entries,
                                                                          const SceneTotals &totals)
{
    std::uint32_t currentVertex{0};
    ProcessedVertices processed;
    processed.vertices.resize(totals.vertices);

    for (const BucketEntry &entry : entries)
    {
        // Particles hijack the vertex, don't overwrite
        const bool updateIndex{entry.shaderType != ShaderType::particles};
        const bool hasSkin{entry.primitive->vertexSkins.has_value()};

        for (std::size_t i{0}; i < entry.primitive->vertices.size(); ++i)
        {
            const Primitive::Vertex &vertex{entry.primitive->vertices[i]};
            const Primitive::Vertex updatedVertex{.position = vertex.position,
                                                  .normal = vertex.normal,
                                                  .tangent = vertex.tangent,
                                                  .texCoords = vertex.texCoords,
                                                  .vboIndex = updateIndex ? currentVertex : vertex.vboIndex,
                                                  .color = vertex.color};

            processed.vertices[currentVertex] = updatedVertex;

            // Save a copy of the original for the compute shader rest positions
            if (hasSkin)
                processed.skinnedVertices.emplace_back(updatedVertex);

            ++currentVertex;
        }
    }

    return processed;
}

BufferBuilder::Sizes SceneProcessor::calculateBufferSizes(const std::vector<DrawElementsIndirectCommand> &commands,
                                                          const std::vector<MultiDrawBucket> &buckets,
                                                          const SceneTotals &totals, bool shadows)
{
    auto alignSSBO = [](const std::size_t buffer, const GLint alignment) {
        std::size_t align{static_cast<std::size_t>(alignment)};

        return ((buffer + align - 1) / align) * align;
    };

    GLint alignment{0};
    glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, &alignment);

    std::size_t drawCounts{buckets.size() * sizeof(std::uint32_t)};
    std::size_t cullResults{commands.size() * sizeof(std::uint32_t)};
    std::size_t visibleInstances{totals.instances * sizeof(std::uint32_t)};
    std::size_t drawCommands{commands.size() * sizeof(DrawElementsIndirectCommand)};

    return {
        .metadata = commands.size() * sizeof(PrimitiveMetadata),
        .matrices = totals.instances * sizeof(glm::mat4),
        .bufferMap = totals.instances * sizeof(VisibilityBufferMap),
        .drawCounts = shadows ? alignSSBO(drawCounts, alignment) : drawCounts,
        .cullResults = shadows ? alignSSBO(cullResults, alignment) : cullResults,
        .visibleInstances = shadows ? alignSSBO(visibleInstances, alignment) : visibleInstances,
        .drawCommands = shadows ? alignSSBO(drawCommands, alignment) : drawCommands,
        .boneTransforms = totals.boneTransforms * sizeof(glm::mat4),
    };
}

std::vector<AABB> SceneProcessor::createAABBs(const BucketEntries &entries, const SceneTotals &totals)
{
    std::vector<AABB> AABBs;
    AABBs.reserve(totals.primitives);

    for (const BucketEntry &entry : entries)
    {
        const AABB &aabb{entry.primitive->aabb};
        AABBs.emplace_back(aabb);
    }

    return AABBs;
}

std::vector<std::set<Handle>> SceneProcessor::createDrawBucketHandles(const BucketEntries &entries,
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

std::vector<DrawElementsIndirectCommand> SceneProcessor::createDrawCommands(const BucketEntries &entries,
                                                                            const SceneTotals &totals)
{
    std::size_t vaoIndex{0};
    std::size_t eboIndex{0};
    std::vector<DrawElementsIndirectCommand> commands;
    commands.reserve(totals.primitives);

    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        const BucketEntry &entry{entries[i]};
        const Primitive &primitive{*entry.primitive};
        const std::size_t numIndices{primitive.indices.size()};

        commands.emplace_back(DrawElementsIndirectCommand{
            .count = static_cast<unsigned int>(numIndices),
            .instanceCount = static_cast<unsigned int>(entry.transforms.size()),
            .firstIndex = static_cast<unsigned int>(eboIndex),
            .baseVertex = static_cast<int>(vaoIndex),
            .baseInstance = static_cast<unsigned int>(i),
        });

        vaoIndex += primitive.vertices.size();
        eboIndex += numIndices;
    }

    return commands;
}

std::vector<MultiDrawBucket> SceneProcessor::createDrawBuckets(const BucketEntries &entries)
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

std::vector<std::vector<std::uint32_t>> SceneProcessor::createMatricesBaseIndexMap(const BucketEntries &entries)
{
    std::uint32_t matricesIndex{0};
    std::vector<std::vector<std::uint32_t>> baseIndices;

    for (const BucketEntry &entry : entries)
    {
        if (entry.handle >= baseIndices.size())
            baseIndices.resize(entry.handle + 1);

        auto &handleMatrices = baseIndices[entry.handle];

        // Resize if a new primitive arrives out of order (i.e. primitive 5 arrives before primitive 3)
        if (entry.originalPrimitiveIndex >= handleMatrices.size())
            handleMatrices.resize(entry.originalPrimitiveIndex + 1);

        // Renderer reads primitives in scene order, not sorted order
        handleMatrices[entry.originalPrimitiveIndex] = matricesIndex;
        matricesIndex += static_cast<std::uint32_t>(entry.transforms.size());
    }

    return baseIndices;
}

VAOs SceneProcessor::createVAO(const BucketEntries &entries, const std::vector<Primitive::Vertex> &vertices,
                               const SceneTotals &totals)
{
    std::vector<std::uint32_t> indices;
    indices.reserve(totals.indices);

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
                                  BufferBuilder::Sizes alignedSizes, bool shadows)
{
    BufferBuilder builder;

    std::size_t cascadeSized{static_cast<std::size_t>(shadows ? shadowCascadeCount + 1 : 1)};

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

std::vector<glm::mat4> SceneProcessor::createMatrices(const BucketEntries &entries, const SceneTotals &totals)
{
    std::vector<glm::mat4> matrices;
    matrices.reserve(totals.instances);

    for (const BucketEntry &entry : entries)
        for (const Transform &transform : entry.transforms)
            matrices.emplace_back(transform.getModelMatrix() * entry.primitive->modelSpaceTransform);

    return matrices;
}

std::vector<PrimitiveMetadata> SceneProcessor::createMetadata(const TextureManager &manager,
                                                              const BucketEntries &entries)
{
    std::vector<PrimitiveMetadata> metadata;
    metadata.resize(entries.size());
    setVisibleInstancesBaseIndex(entries, metadata);
    setMetadataMaterials(manager, entries, metadata);

    return metadata;
}

void SceneProcessor::setVisibleInstancesBaseIndex(const BucketEntries &entries,
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

std::size_t SceneProcessor::getParticleBucketStart(const BucketEntries &entries)
{
    std::size_t vboIndex{0};

    for (const BucketEntry &entry : entries)
        if (entry.shaderType == ShaderType::particles)
            break;
        else
            vboIndex += entry.primitive->vertices.size();

    return vboIndex;
}

void SceneProcessor::setMetadataMaterials(const TextureManager &manager, const BucketEntries &entries,
                                          std::vector<PrimitiveMetadata> &metadata)
{
    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        const BucketEntry &entry{entries[i]};
        const std::size_t matIndex{entry.primitive->materialIndex};
        std::span<const Material> materials{manager.getMaterials(entry.handle)};

        if (matIndex >= materials.size())
        {
            std::cerr << "Material index out of bound for handle " << entry.handle << std::endl;
            throw std::runtime_error("Material index out of bounds");
        }

        PrimitiveMetadata &metaEntry{metadata[i]};
        const Material &material{materials[matIndex]};

        metaEntry.albedo = material.layers[Material::albedo];
        metaEntry.ARM = material.layers[Material::ARM];
        metaEntry.normal = material.layers[Material::normal];
        metaEntry.cubemap = material.layers[Material::cubemap];
    }
}

std::vector<VisibilityBufferMap> SceneProcessor::createVisibilityBufferMap(
    const std::vector<SceneProcessor::BucketEntry> &entries, const std::vector<PrimitiveMetadata> &metadata,
    const std::vector<MultiDrawBucket> &buckets, const SceneTotals &totals)
{
    std::vector<VisibilityBufferMap> map;
    map.reserve(totals.instances);

    for (std::size_t i{0}; i < buckets.size(); ++i)
    {
        const MultiDrawBucket &bucket{buckets[i]};

        for (std::uint32_t j{bucket.drawCommandsBaseIndex}; j < bucket.drawCommandsBaseIndex + bucket.maxDrawCalls; ++j)
        {
            const PrimitiveMetadata &metaEntry{metadata[j]};
            std::span<const Transform> transforms{entries[j].transforms};

            const std::uint32_t visibleInstancesBufferBaseIndex{metaEntry.visibleInstancesBaseIndex};
            const std::uint32_t currentBucket{static_cast<std::uint32_t>(i)};

            for (std::size_t k{0}; k < transforms.size(); ++k)
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

std::vector<std::uint32_t> SceneProcessor::createParticleMap(const BucketEntries &entries, const SceneTotals &totals)
{
    std::vector<std::uint32_t> particleMap;
    particleMap.reserve(totals.particles);

    for (std::size_t i{0}; i < entries.size(); ++i)
    {
        if (entries[i].shaderType != ShaderType::particles)
            continue;

        for (std::size_t j{0}; j < entries[i].primitive->vertices.size(); ++j)
            particleMap.emplace_back(static_cast<std::uint32_t>(i));
    }

    return particleMap;
}

std::unordered_map<Handle, std::uint32_t> SceneProcessor::createBoneTransformMap(const BucketEntries &entries)
{
    std::unordered_map<Handle, std::uint32_t> boneTransformsMap;
    std::uint32_t boneBufferOffset{0};

    for (const BucketEntry &entry : entries)
    {
        // Check for vertex skins specifically, since animations can exist solely as node transforms
        if (!entry.primitive->vertexSkins.has_value())
            continue;

        auto [iter, inserted] = boneTransformsMap.try_emplace(entry.handle, boneBufferOffset);

        if (inserted)
        {
            const auto &skin = entry.animation->skeletalAnimation.skeleton.skin;
            boneBufferOffset += static_cast<std::uint32_t>(skin->invBindMatrices.size());
        }
    }

    return boneTransformsMap;
}

std::vector<std::uint32_t> SceneProcessor::createMetadataBridge(const BucketEntries &entries)
{
    std::vector<std::uint32_t> bridge(entries.size());

    for (std::size_t i{0}; i < entries.size(); ++i)
        bridge[entries[i].originalSceneIndex] = static_cast<std::uint32_t>(i);

    return bridge;
}

std::vector<Primitive::VertexSkin> SceneProcessor::createVertexSkins(
    const BucketEntries &entries, const std::unordered_map<Handle, std::uint32_t> &boneTransformMap,
    const SceneTotals &totals)
{
    std::vector<Primitive::VertexSkin> skinData;
    skinData.reserve(totals.skinnedVertices);

    for (const BucketEntry &entry : entries)
    {
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

        for (std::size_t i{0}; i < entry.primitive->vertices.size(); ++i)
        {
            glm::uvec4 boneIDs{vertexSkins[i].boneIDs};

            for (std::size_t j{0}; j < 4; ++j)
                if (vertexSkins[i].weights[j] > 0.0f)
                    boneIDs[j] += static_cast<unsigned int>(bufferOffset);

            skinData.push_back({vertexSkins[i].weights, boneIDs});
        }
    }

    return skinData;
}
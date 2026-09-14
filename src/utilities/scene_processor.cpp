#include "utilities/scene_processor.h"

#include <algorithm>
#include <cstdint>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
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
#include "utilities/random.h"
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

        hasGeometry = true;

        for (const Primitive &primitive : entry.primitives)
        {
            if (primitive.shaderType == ocean || primitive.shaderType == skybox)
                continue;

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
    auto expVertsMap = createExplodingVerticesMap(bucketEntries, totals);
    auto processedVertices = createProcessedVertices(bucketEntries, totals);
    auto drawBucketHandles = createDrawBucketHandles(bucketEntries, buckets);
    auto vertexSkins = createVertexSkins(bucketEntries, boneTransformMap, totals);
    auto bufferSizes = calculateBufferSizes(commands, buckets, totals, shadowsEnabled);
    auto visibilityMap = createVisibilityBufferMap(bucketEntries, metadata, buckets, totals);

    // Stage Four - GPU data upload
    auto VAO = createVAO(bucketEntries, processedVertices.vertices, totals);
    auto SSBOs = createSSBOs(commands, matrices, metadata, vertexSkins, processedVertices.skinnedVertices,
                             visibilityMap, expVertsMap, particleMap, AABBs, bufferSizes, shadowsEnabled);

    return {.VAOs = std::move(VAO),
            .SSBOs = std::move(SSBOs),
            .sceneDescriptors =
                {
                    .worldAABB = sceneAABB,
                    .numInstances = totals.instances,
                    .numVertices = totals.vertices,
                    .numSkinnedVertices = totals.skinnedVertices,
                    .numExplodedVertices = totals.explodedVertices,
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

        const bool hasSkeleton{entry.entryAnimation && entry.entryAnimation->getNumJoints()};

        for (const Primitive &primitive : entry.primitives)
        {
            std::size_t numVerts{primitive.vertices.size()};

            totals.indices += primitive.indices.size();
            totals.vertices += numVerts;

            if (primitive.shaderType == ShaderType::particles)
                totals.particles += numVerts;

            if (primitive.vertexSkins)
                totals.skinnedVertices += numVerts;

            if (primitive.explosionDistances)
                totals.explodedVertices += numVerts;
        }

        if (!hasSkeleton)
            continue;

        totals.boneTransforms += entry.entryAnimation->getNumJoints();
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
                                                   .animation = &entry.entryAnimation,
                                                   .shaderType = primitive.shaderType,
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
    processed.skinnedVertices.reserve(totals.skinnedVertices);

    for (const BucketEntry &entry : entries)
    {
        // Particles hijack the vertex, don't overwrite any properties
        const bool updateIndex{entry.shaderType != ShaderType::particles};
        const bool hasSkin{entry.primitive->vertexSkins};

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
            buckets.emplace_back(MultiDrawBucket{
                .maxDrawCalls = end - start,
                .drawCommandsBaseIndex = start,
                .state = createBucketState(currentShaderType, currentAlphaMode),
            });

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
            .state = createBucketState(currentShaderType, currentAlphaMode),
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

SSBOs SceneProcessor::createSSBOs(
    const std::vector<DrawElementsIndirectCommand> &commands, const std::vector<glm::mat4> &matrices,
    const std::vector<PrimitiveMetadata> &metadata, const std::vector<Primitive::VertexSkin> &vertexSkins,
    const std::vector<Primitive::Vertex> &originalSkinnedVerts, const std::vector<VisibilityBufferMap> &visibilityMap,
    const std::vector<ExplodingVertexMap> &expVertMap, const std::vector<std::uint32_t> &particleMap,
    const std::vector<AABB> &aabbs, BufferBuilder::Sizes alignedSizes, bool shadows)
{
    BufferBuilder builder;

    std::size_t cascadeSized{static_cast<std::size_t>(shadows ? CascadeShadow::cascadeCount + 1 : 1)};

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
                              expVertMap, particleMap, aabbs, sizes);
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
        if (!entry.primitive->vertexSkins)
            continue;

        auto [iter, inserted] = boneTransformsMap.try_emplace(entry.handle, boneBufferOffset);

        if (inserted)
            boneBufferOffset += static_cast<std::uint32_t>(entry.animation->value().getNumJoints());
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
        if (!entry.primitive->vertexSkins)
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

            for (glm::length_t j{0}; j < 4; ++j)
                if (vertexSkins[i].weights[j] > 0.0f)
                    boneIDs[j] += static_cast<unsigned int>(bufferOffset);

            skinData.push_back({vertexSkins[i].weights, boneIDs});
        }
    }

    return skinData;
}

std::vector<ExplodingVertexMap> SceneProcessor::createExplodingVerticesMap(const SceneProcessor::BucketEntries &entries,
                                                                           const SceneTotals &totals)
{
    std::vector<ExplodingVertexMap> map;
    map.reserve(totals.explodedVertices);
    std::uint32_t vboIndex{0};
    std::uint32_t matricesIndex{0};

    for (const BucketEntry &entry : entries)
    {
        const std::uint32_t numMatrices{static_cast<std::uint32_t>(entry.transforms.size())};
        const std::uint32_t numVerts{static_cast<std::uint32_t>(entry.primitive->vertices.size())};

        // Doesn't work for instanced, because all instances share the vertices and the explosion effect cannot
        // distinguish one instance from another
        if (!entry.primitive->explosionDistances || numMatrices > 1)
        {
            vboIndex += numVerts;
            matricesIndex += numMatrices;
            continue;
        }

        const std::uint32_t packedDist{glm::packHalf2x16(glm::vec2{entry.primitive->explosionDistances->minDistance,
                                                                   entry.primitive->explosionDistances->maxDistance})};

        // All verts have been expanded from the indices list, so vertex count == indices count and each vertex gets a
        // proper (non shared) entry. Indices are identity at this point (0, 1, 2, 3, ..., n)
        for (std::uint32_t i{0}; i < numVerts; i += 3)
        {
            const glm::vec3 a{entry.primitive->vertices[i].position};
            const glm::vec3 b{entry.primitive->vertices[i + 1].position};
            const glm::vec3 c{entry.primitive->vertices[i + 2].position};      
            glm::vec3 centroidNormal{glm::cross(b - a, c - a)};

            if (glm::length(centroidNormal) > 0.0001f)
                centroidNormal = glm::normalize(centroidNormal);
            else
                centroidNormal = WorldVectors::worldUp;

            const std::uint32_t packedNorm{glm::packSnorm3x10_1x2(glm::vec4{centroidNormal, 0.0f})};

            for (std::uint32_t j{0}; j < 3; ++j)
            {
                const Primitive::Vertex &vertex{entry.primitive->vertices[i + j]};
                map.emplace_back(vertex.position, vboIndex + i + j, matricesIndex, packedNorm, vertex.normal,
                                 packedDist);
            }
        }

        ++matricesIndex;
        vboIndex += numVerts;
    }

    return map;
}
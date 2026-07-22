#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <set>
#include <span>
#include <unordered_map>
#include <vector>

#include "models/aabb.h"
#include "models/primitive.h"
#include "renderer/buffers.h"
#include "renderer/renderer_types.h"
#include "renderer/shader/graphics_shader.h"
#include "scene/handle.h"
#include "scene/scene.h"
#include "utilities/buffer_builder.h"
#include "utilities/texture_manager.h"
#include "utilities/transform.h"

class SceneProcessor
{
  public:
    struct SceneDescriptor
    {
        AABB worldAABB;
        std::size_t numInstances{0};
        std::size_t numVertices{0};
        std::size_t numSkinnedVertices{0};
        std::size_t numPrimitives{0};
        std::size_t numParticles{0};
    };

    struct BufferDescriptor
    {
        std::size_t drawCommandsSize{0};
        std::size_t cullResultsSize{0};
        std::size_t drawCountsSize{0};
        std::size_t visibleInstancesSize{0};
        std::size_t particleBaseVBOIndex{0};
        std::vector<std::uint32_t> metadataBridge;
        std::vector<std::set<Handle>> drawBucketHandles;
        std::vector<std::vector<std::uint32_t>> matricesBaseIndexMap;
        std::unordered_map<Handle, std::uint32_t> boneTransformsBaseIndexMap;
    };

    struct ProcessedScene
    {
        VAOs VAOs;
        SSBOs SSBOs;
        SceneDescriptor sceneDescriptors;
        BufferDescriptor bufferDescriptors;
        std::vector<MultiDrawBucket> drawBuckets;
    };

    [[nodiscard]] static ProcessedScene processScene(const Scene &scene, const TextureManager &textureManager,
                                                     bool shadowsEnabled);

  private:
    // For counting and reserving scene related sizes
    struct SceneTotals
    {
        std::size_t vertices{0};
        std::size_t indices{0};
        std::size_t instances{0};
        std::size_t primitives{0};
        std::size_t particles{0};
        std::size_t boneTransforms{0};
        std::size_t skinnedVertices{0};
    };

    // Intermediate step in building the final Draw Commands
    struct BucketEntry
    {
        Handle handle;
        const Primitive *primitive;
        std::span<const Transform> transforms;
        std::optional<Scene::EntryAnimation> animation;

        ShaderType shaderType{ShaderType::directionalPBR};
        AlphaMode alphaMode{AlphaMode::opaque};

        std::size_t originalSceneIndex{0};
        std::size_t originalPrimitiveIndex{0};
    };

    struct ProcessedVertices
    {
        std::vector<Primitive::Vertex> vertices;
        std::vector<Primitive::Vertex> skinnedVertices;
    };

    using BucketEntries = std::vector<BucketEntry>;

    // Setup
    [[nodiscard]] static SceneTotals getSceneTotals(const Scene &scene);
    [[nodiscard]] static BucketEntries createBucketEntries(const Scene &scene, const SceneTotals &totals);

    // Bucket Entry only
    [[nodiscard]] static std::size_t getParticleBucketStart(const BucketEntries &entries);
    [[nodiscard]] static std::vector<MultiDrawBucket> createDrawBuckets(const BucketEntries &entries);
    [[nodiscard]] static std::vector<std::uint32_t> createMetadataBridge(const BucketEntries &entries);
    [[nodiscard]] static std::unordered_map<Handle, std::uint32_t> createBoneTransformMap(const BucketEntries &entries);
    [[nodiscard]] static std::vector<std::vector<std::uint32_t>> createMatricesBaseIndexMap(
        const BucketEntries &entries);

    // Multiple dependencies
    [[nodiscard]] static std::vector<AABB> createAABBs(const BucketEntries &entries, const SceneTotals &totals);
    [[nodiscard]] static std::vector<glm::mat4> createMatrices(const BucketEntries &entries, const SceneTotals &totals);

    [[nodiscard]] static std::vector<std::set<Handle>> createDrawBucketHandles(
        const BucketEntries &entries, const std::vector<MultiDrawBucket> &drawBuckets);

    [[nodiscard]] static ProcessedVertices createProcessedVertices(const BucketEntries &entries,
                                                                   const SceneTotals &totals);

    [[nodiscard]] static std::vector<std::uint32_t> createParticleMap(const BucketEntries &entries,
                                                                      const SceneTotals &totals);

    [[nodiscard]] static std::vector<PrimitiveMetadata> createMetadata(const TextureManager &manager,
                                                                       const BucketEntries &entries);

    [[nodiscard]] static std::vector<DrawElementsIndirectCommand> createDrawCommands(const BucketEntries &entries,
                                                                                     const SceneTotals &totals);

    [[nodiscard]] static std::vector<Primitive::VertexSkin> createVertexSkins(
        const BucketEntries &entries, const std::unordered_map<Handle, std::uint32_t> &boneTransformMap,
        const SceneTotals &totals);

    [[nodiscard]] static std::vector<VisibilityBufferMap> createVisibilityBufferMap(
        const std::vector<SceneProcessor::BucketEntry> &entries, const std::vector<PrimitiveMetadata> &metadata,
        const std::vector<MultiDrawBucket> &buckets, const SceneTotals &totals);

    [[nodiscard]] static BufferBuilder::Sizes calculateBufferSizes(
        const std::vector<DrawElementsIndirectCommand> &commands, const std::vector<MultiDrawBucket> &buckets,
        const SceneTotals &totals, bool shadows);

    // GPU upload
    [[nodiscard]] static VAOs createVAO(const BucketEntries &entries, const std::vector<Primitive::Vertex> &vertices,
                                        const SceneTotals &totals);
    [[nodiscard]] static SSBOs createSSBOs(
        const std::vector<DrawElementsIndirectCommand> &commands, const std::vector<glm::mat4> &matrices,
        const std::vector<PrimitiveMetadata> &metadata, const std::vector<Primitive::VertexSkin> &vertexSkins,
        const std::vector<Primitive::Vertex> &originalSkinnedVerts,
        const std::vector<VisibilityBufferMap> &visibilityMap, const std::vector<std::uint32_t> &particleMap,
        const std::vector<AABB> &aabbs, BufferBuilder::Sizes alignedSizes, bool shadows);

    // Metadata helpers
    static void setMetadataMaterials(const TextureManager &manager, const BucketEntries &entries,
                                     std::vector<PrimitiveMetadata> &metadata);
    static void setVisibleInstancesBaseIndex(const BucketEntries &entries, std::vector<PrimitiveMetadata> &metadata);
};
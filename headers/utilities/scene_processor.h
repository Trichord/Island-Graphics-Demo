#pragma once

#include <cstdint>
#include <glad/glad.h>
#include <glm/glm.hpp>
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

    SceneProcessor(Scene &scene, TextureManager &textureManager, bool shadowsEnabled);

    std::size_t getNumDynamicCubemaps() const;
    [[nodiscard]] ProcessedScene processScene() const;

  private:
    // For counting and reserving scene related sizes
    struct SceneTotals
    {
        std::size_t vertices{0};
        std::size_t indices{0};
        std::size_t instances{0};
        std::size_t primitives{0};
        std::size_t dynamicCubemaps{0};
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

        std::size_t originalIndex{0};
    };

    struct ProcessedVertices
    {
        std::vector<Primitive::Vertex> vertices;
        std::vector<Primitive::Vertex> skinnedVertices;
    };

    bool shadows_{false};
    AABB sceneAABB_;
    SceneTotals totals_;
    std::vector<BucketEntry> bucketEntries_;

    TextureManager &textureManager_;

    // Setup
    SceneTotals getSceneTotals(Scene &scene) const;
    std::vector<BucketEntry> createBucketEntries(Scene &scene, const SceneTotals &totals) const;

    // Bucket Entry only
    [[nodiscard]] std::vector<AABB> createAABBs(const std::vector<BucketEntry> &entries) const;
    [[nodiscard]] std::size_t getParticleBucketStart(const std::vector<BucketEntry> &entries) const;
    [[nodiscard]] std::vector<glm::mat4> createMatrices(const std::vector<BucketEntry> &entries) const;
    [[nodiscard]] ProcessedVertices createProcessedVertices(const std::vector<BucketEntry> &entries) const;
    [[nodiscard]] std::vector<std::uint32_t> createParticleMap(const std::vector<BucketEntry> &entries) const;
    [[nodiscard]] std::vector<PrimitiveMetadata> createMetadata(const std::vector<BucketEntry> &entries) const;
    [[nodiscard]] std::vector<MultiDrawBucket> createDrawBuckets(const std::vector<BucketEntry> &entries) const;
    [[nodiscard]] std::vector<std::uint32_t> createMetadataBridge(const std::vector<BucketEntry> &entries) const;
    [[nodiscard]] std::vector<DrawElementsIndirectCommand> createDrawCommands(
        const std::vector<BucketEntry> &entries) const;
    [[nodiscard]] std::vector<std::vector<std::uint32_t>> createMatricesBaseIndexMap(
        const std::vector<BucketEntry> &entries) const;
    [[nodiscard]] std::unordered_map<Handle, std::uint32_t> createBoneTransformMap(
        const std::vector<BucketEntry> &entries) const;

    // Multi vector
    [[nodiscard]] std::vector<Primitive::VertexSkin> createVertexSkins(
        const std::vector<BucketEntry> &entries,
        const std::unordered_map<Handle, std::uint32_t> &boneTransformMap) const;
    [[nodiscard]] std::vector<std::set<Handle>> createDrawBucketHandles(
        const std::vector<BucketEntry> &entries, const std::vector<MultiDrawBucket> &drawBuckets) const;
    [[nodiscard]] std::vector<VisibilityBufferMap> createVisibilityBufferMap(
        const std::vector<BucketEntry> &entries, const std::vector<PrimitiveMetadata> &metadata,
        const std::vector<MultiDrawBucket> &buckets) const;
    [[nodiscard]] BufferBuilder::Sizes calculateBufferSizes(const std::vector<DrawElementsIndirectCommand> &commands,
                                                            const std::vector<MultiDrawBucket> &buckets) const;

    // GPU upload
    [[nodiscard]] VAOs createVAO(const std::vector<BucketEntry> &entries,
                                 const std::vector<Primitive::Vertex> &vertices) const;
    [[nodiscard]] SSBOs createSSBOs(const std::vector<DrawElementsIndirectCommand> &commands,
                                    const std::vector<glm::mat4> &matrices,
                                    const std::vector<PrimitiveMetadata> &metadata,
                                    const std::vector<Primitive::VertexSkin> &vertexSkins,
                                    const std::vector<Primitive::Vertex> &originalSkinnedVerts,
                                    const std::vector<VisibilityBufferMap> &visibilityMap,
                                    const std::vector<std::uint32_t> &particleMap, const std::vector<AABB> &aabbs,
                                    BufferBuilder::Sizes alignedSizes) const;
    // Metadata helpers
    void setMetadataMaterials(const std::vector<BucketEntry> &entries, std::vector<PrimitiveMetadata> &metadata) const;
    void setVisibleInstancesBaseIndex(const std::vector<BucketEntry> &entries,
                                      std::vector<PrimitiveMetadata> &metadata) const;
};
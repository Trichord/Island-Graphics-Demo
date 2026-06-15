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
        std::size_t numPrimitives{0};
        std::size_t numParticles{0};
    };

    struct BufferDescriptor
    {
        std::size_t drawCommandsSize{0};
        std::size_t cullResultsSize{0};
        std::size_t drawCountsSize{0};
        std::size_t visibleInstancesSize{0};
        std::vector<std::vector<std::uint32_t>> matricesBaseIndexMap;
        std::vector<std::set<Handle>> drawBucketHandles;
    };

    struct ProcessedScene
    {
        VAOs VAOs;
        SSBOs SSBOs;
        SceneDescriptor sceneDescriptor;
        BufferDescriptor bufferDescriptor;
        std::vector<MultiDrawBucket> drawBuckets;
    };

    SceneProcessor(Scene &scene, TextureManager &textureManager, bool shadowsEnabled);

    std::size_t getNumDynamicCubemaps() const;
    [[nodiscard]] ProcessedScene processScene();
    void updateMetadataCubemaps(GLuint metadataBufferID);

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
    };

    // Intermediate step in building the final Draw Commands
    struct BucketEntry
    {
        Handle handle;
        Primitive *primitive;
        std::span<Transform> transforms;

        ShaderType shaderType{ShaderType::directionalPBR};
        AlphaMode alphaMode{AlphaMode::opaque};
    };

    bool shadows_{false};
    AABB sceneAABB_;
    SceneTotals totals_;
    std::vector<BucketEntry> bucketEntries_;
    std::vector<PrimitiveMetadata> metadata_;
    std::unordered_map<Handle, std::uint32_t> matricesBaseIndex_;

    TextureManager &textureManager_;

    // Setup
    SceneTotals getSceneTotals(Scene &scene);
    std::vector<BucketEntry> createDrawEntries(Scene &scene);

    // Counting
    [[nodiscard]] BufferBuilder::Sizes calculateBufferSizes(const std::vector<DrawElementsIndirectCommand> &commands,
                                                            const std::vector<MultiDrawBucket> &buckets);

    // Stage One (draw entry only)
    [[nodiscard]] std::vector<AABB> createAABBs(const std::vector<BucketEntry> &entries);
    [[nodiscard]] std::vector<glm::mat4> createMatrices(const std::vector<BucketEntry> &entries);
    [[nodiscard]] std::vector<PrimitiveMetadata> createMetadata(const std::vector<BucketEntry> &entries);
    [[nodiscard]] std::vector<MultiDrawBucket> createDrawBuckets(const std::vector<BucketEntry> &entries);
    [[nodiscard]] std::vector<VertexBufferMap> createVertexBufferMap(const std::vector<BucketEntry> &entries);
    [[nodiscard]] std::vector<DrawElementsIndirectCommand> createDrawCommands(const std::vector<BucketEntry> &entries);
    [[nodiscard]] std::vector<std::vector<std::uint32_t>> createMatricesBaseIndexMap(
        const std::vector<BucketEntry> &entries);

    // Stage two (multi-vector dependent)
    [[nodiscard]] std::vector<std::set<Handle>> createDrawBucketHandles(
        const std::vector<BucketEntry> &entries, const std::vector<MultiDrawBucket> &drawBuckets);
    [[nodiscard]] std::vector<VisibilityBufferMap> createVisibilityBufferMap(
        const std::vector<BucketEntry> &entries, const std::vector<PrimitiveMetadata> &metadata,
        const std::vector<MultiDrawBucket> &buckets);

    // GPU data creation
    [[nodiscard]] VAOs createVAO(const std::vector<BucketEntry> &entries);
    [[nodiscard]] SSBOs createSSBOs(const std::vector<DrawElementsIndirectCommand> &commands,
                                    const std::vector<glm::mat4> &matrices,
                                    const std::vector<PrimitiveMetadata> &metadata,
                                    const std::vector<VisibilityBufferMap> &visibilityMap,
                                    const std::vector<VertexBufferMap> &vertexMap, const std::vector<AABB> &aabbs,
                                    BufferBuilder::Sizes sizes);
    // Metadata helpers
    void setMetadataMaterials(const std::vector<BucketEntry> &entries, std::vector<PrimitiveMetadata> &metadata);
    void setVisibleInstancesBaseIndex(const std::vector<BucketEntry> &entries,
                                      std::vector<PrimitiveMetadata> &metadata);
};
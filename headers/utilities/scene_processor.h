#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <unordered_map>
#include <vector>

#include "aabb.h"
#include "buffer_builder.h"
#include "buffers.h"
#include "fastgltf/types.hpp"
#include "glm/glm.hpp"
#include "graphics_shader.h"
#include "handle.h"
#include "primitive.h"
#include "renderer_types.h"
#include "scene.h"
#include "texture_manager.h"
#include "transform.h"

class SceneProcessor
{
  public:
    struct SceneData
    {
        AABB worldAABB;
        std::size_t numInstances{0};
    };

    struct BufferData
    {
        std::vector<MultiDrawBucket> buckets;
        std::unordered_map<Handle, std::uint32_t> matricesBaseIndexMap;
    };

    // Maps the starting and ending position in the draw commands list, and all the handles contained within that bucket
    struct BucketData
    {
        std::size_t startIndex{0};
        std::size_t endIndex{0};
        std::set<Handle> handles;
        std::vector<size_t> vertexCount;
    };

    struct ProcessedScene
    {
        SSBOs ssbos;
        GlobalVAO vao;
        SceneData sceneData;
        BufferData bufferData;
        std::unordered_map<ShaderType, BucketData> bucketMap;
        std::vector<DrawElementsIndirectCommand> commands;
    };

    SceneProcessor(Scene &scene, TextureManager &textureManager);

    std::size_t getNumDynamicCubemaps() const;
    [[nodiscard]] ProcessedScene processScene();
    void updateMetadataCubemaps(GLuint metadataBufferID);

  private:
    // For counting and reserving scene related sizes
    struct SceneTotals
    {
        std::size_t vertices{0};
        std::size_t indices{0};
        std::size_t matrices{0};
        std::size_t instances{0};
        std::size_t primitives{0};
        std::size_t dynamicCubemaps{0};
    };

    // Intermediate step in building the final Draw Commands
    struct DrawEntry
    {
        Handle handle;
        Primitive *primitive;
        std::span<Transform> transforms;

        ShaderType shaderType{ShaderType::directionalPBR};
        fastgltf::AlphaMode alphaMode{fastgltf::AlphaMode::Opaque};
    };

    // GPU resident metadata for texture and buffer indexing
    struct PrimitiveMetadata
    {
        int albedoArray{0};
        int albedoLayer{0};
        int armArray{0};
        int armLayer{0};
        int normalArray{0};
        int normalLayer{0};
        int cubemapArray{0};
        int cubemapLayer{0};
        std::uint32_t visibleInstancesBaseIndex{0};
    };

    // Maps an entry in the visibility buffer, so that a compute shader global ID can find its resources
    struct BufferMap
    {
        std::uint32_t matricesIndex{0};
        std::uint32_t sharedBaseIndex{0};
        std::uint32_t drawBucket{0};
        std::uint32_t drawCommandsBaseIndex{0};
        std::uint32_t visibleInstancesBaseIndex{0};
    };

    AABB sceneAABB_;
    SceneTotals totals_;
    std::vector<DrawEntry> drawEntries_;
    std::vector<PrimitiveMetadata> metadata_;
    std::unordered_map<Handle, std::uint32_t> matricesBaseIndex_;

    TextureManager &textureManager_;

    // Setup
    SceneTotals getSceneTotals(Scene &scene);
    std::vector<DrawEntry> createDrawEntries(Scene &scene);
    [[nodiscard]] BufferBuilder::Sizes calculateBufferSizes(const std::vector<DrawElementsIndirectCommand> &commands,
                                                            const std::vector<MultiDrawBucket> &buckets);

    // Staging
    [[nodiscard]] std::unordered_map<ShaderType, BucketData> createShaderBucketMap();
    [[nodiscard]] std::vector<AABB> createAABBs(const std::vector<DrawEntry> &entries);
    [[nodiscard]] std::vector<DrawElementsIndirectCommand> createDrawCommands(
        const std::vector<DrawEntry> &drawEntries);
    [[nodiscard]] std::vector<MultiDrawBucket> createDrawBuckets(const std::vector<DrawEntry> &entries);
    [[nodiscard]] std::unordered_map<Handle, std::uint32_t> createMatricesBaseIndexMap(
        const std::vector<DrawEntry> &entries);

    // Buffers
    [[nodiscard]] GlobalVAO createVAO(const std::vector<DrawEntry> &entries);
    [[nodiscard]] SSBOs createSSBOs(const std::vector<DrawElementsIndirectCommand> &commands,
                                    const std::vector<AABB> &AABBs, const BufferBuilder::Sizes bufferSizes);

    // GPU data creation
    [[nodiscard]] std::vector<glm::mat4> createMatrices(const std::vector<DrawEntry> &entries);
    [[nodiscard]] std::vector<PrimitiveMetadata> createMetadata(const std::vector<DrawEntry> &entries);
    [[nodiscard]] std::vector<BufferMap> createBufferMap(const std::vector<SceneProcessor::DrawEntry> &entries,
                                                         const std::vector<SceneProcessor::PrimitiveMetadata> &metadata,
                                                         const std::vector<MultiDrawBucket> &buckets,
                                                         const std::unordered_map<Handle, std::uint32_t> matricesMap);

    // Metadata helpers
    void setMetadataMaterials(const std::vector<DrawEntry> &entries, std::vector<PrimitiveMetadata> &metadata);
    void setVisibleInstancesBaseIndex(const std::vector<DrawEntry> &entries, std::vector<PrimitiveMetadata> &metadata);
};
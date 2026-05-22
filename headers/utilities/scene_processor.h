#pragma once

#include <cstdint>
#include <optional>
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
    struct ProcessedScene
    {
        SSBOs ssbos;
        GlobalVAO vao;
        std::vector<MultiDrawBucket> buckets;
        std::vector<ParticleCompute> particleInfo;
        std::vector<DrawElementsIndirectCommand> commands;
        std::unordered_map<Handle, std::uint32_t> matricesBaseIndexMap;

        AABB worldAABB;
        std::size_t numInstances{0};
        std::size_t numPrimitives{0};
    };

    struct ShaderBucketInfo
    {
        std::size_t size{0};
        std::size_t drawCommandsStartIndex{0};
        std::vector<Handle> handles;
    };

    SceneProcessor(Scene &scene, TextureManager &textureManager);

    std::size_t getNumDynamicCubemaps() const;

    std::optional<Handle> getSkyboxHandle() const;
    std::optional<ShaderBucketInfo> getShaderBucketInfo(ShaderType shader) const;

    void updateMetadataCubemaps(GLuint metadataBufferID);
    [[nodiscard]] ProcessedScene processScene();

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

    SSBOs SSBOs_;
    GlobalVAO vao_;
    TextureManager &textureManager_;

    // Sizes
    AABB sceneAABB_;
    SceneTotals sceneTotals_;
    BufferBuilder::Sizes bufferSizes_;

    // Intermediate between a scene and the final GPU data structures
    std::vector<DrawEntry> drawEntries_;

    // Eventual buffer data
    std::vector<AABB> AABBs_;
    std::vector<glm::mat4> matrices_;
    std::vector<BufferMap> bufferMap_;
    std::vector<PrimitiveMetadata> metadata_;
    std::vector<MultiDrawBucket> drawBuckets_;
    std::vector<DrawElementsIndirectCommand> drawCommands_;

    // Buffer management - help find base index into matrices buffer for model updates
    std::unordered_map<Handle, std::uint32_t> matricesBaseIndex_;

    // Particle indirection tracking for CPU-side uniform setting
    std::vector<ParticleCompute> particles_;

    bool sceneProcessed_{false};
    bool buffersBuilt_{false};

    void createDrawEntries(Scene &scene);

    // Counting
    void calculateBufferSizes();
    SceneTotals getSceneTotals(Scene &scene);

    // Buffer data
    void createAABBs();
    void createDrawBuckets();
    void createDrawCommands();
    void createParticleInfo();
    void createAndUploadMatrices();
    void createAndUploadBufferMap();

    // Buffers
    void createVAO();
    void createSSBOHandles();

    // Metadata creation
    void createAndUploadMetadata();
    void setMetadataMaterials();
    void setVisibleInstancesBaseIndex();
};
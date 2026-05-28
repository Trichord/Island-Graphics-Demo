#pragma once

#include <array>
#include <cstdint>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

#include "application/graphics_settings.h"
#include "renderer/buffers.h"
#include "renderer/cascade_shadow.h"
#include "renderer/gl_state.h"
#include "renderer/render_target.h"
#include "renderer/renderer_types.h"
#include "renderer/shader/compute_shader.h"
#include "renderer/shader/graphics_shader.h"
#include "scene/handle.h"
#include "scene/scene.h"
#include "utilities/scene_processor.h"
#include "utilities/shader_bucket_hash.h"
#include "utilities/texture_manager.h"
#include "utilities/thread_pool.h"

class Renderer
{
  public:
    Renderer(ThreadPool &threadPool, TextureManager &textureManager, const GraphicsSettings &settings);
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    Renderer(Renderer &&other) = delete;
    Renderer &operator=(Renderer &&other) = delete;

    void initializeResources(Scene &scene);
    void renderScene(Scene &scene, float deltaTime);

    std::uint32_t getFrameCount() const;
    TextureManager &getTextureManager();

  private:
    using BucketMap = std::unordered_map<ShaderBucket, SceneProcessor::BucketData, ShaderBucketHash>;

    std::uint32_t framesRendered_{0};
    static constexpr float depthClear_{1.0f};
    static constexpr std::array<float, 4> clearColor_{0.0f, 0.0f, 0.0f, 1.0f};

    // Critical members
    ThreadPool &threadPool_;
    TextureManager &textureManager_;
    const GraphicsSettings graphicsSettings_;

    // Owned objects
    GLState glState_;
    std::vector<glm::mat4> updatedMatrices_;
    std::unordered_map<Handle, std::uint32_t> matricesBaseIndex_;
    std::array<GraphicsShader, static_cast<int>(ShaderType::count)> shaders_;

    // GPU Buffers
    UBOs UBOs_;
    SSBOs SSBOs_;
    VAOs VAOs_;

    // Textures
    CascadeShadow cascadeShadow_;
    RenderTarget shadowPassTarget_;

    // Drawing
    std::vector<MultiDrawBucket> drawBuckets_;

    // Compute Shaders
    ComputeShader cullShader_;
    ComputeShader compactionShader_;
    ComputeShader particleShader_;
    std::uint32_t cullWorkGroupSize_{0};
    std::uint32_t compactionWorkGroupSize_{0};
    std::uint32_t particleWorkGroupSize_{0};

    // Initialization
    void loadShaders();
    void setupShader(GraphicsShader &shader);

    // Setup
    void setSceneShaderUniforms(Scene &scene);
    void takeBufferData(SceneProcessor::ProcessedScene &processed);
    void takeGPUObjects(SceneProcessor::ProcessedScene &processed);
    void setLocalGroupSizes(const SceneProcessor::ProcessedScene &processed,
                            const std::vector<DrawElementsIndirectCommand> &commands,
                            const std::vector<std::size_t> vertexCounts, const BucketMap &bucketMap);

    // Cubemap Generation
    void borrowSkybox(const BucketMap &bucketMap);
    void createCubemaps(Scene &scene, const std::vector<DrawElementsIndirectCommand> &commandsSource,
                        const BucketMap &bucketMap);
    void renderCubeFaces(Scene &scene, RenderTarget &target, RenderTarget &blitTarget);

    // Drawing
    void renderScene();
    void cullScene(const glm::mat4 &cullMatrix);
    void renderShadowMaps(Scene &scene, bool lightChanged);
    void shadowPass(GraphicsShader &shader, int cascade, bool alphaTested);

    // Updates
    void updateTimeUBO(float deltaTime);
    void updateParticles(float deltaTime);
    void updateCameraUBO(Scene &scene);
    void updateSceneTransforms(Scene &scene);
    void updateFrustumPlanesUBO(const glm::mat4 &cullMatrix);
    void updateShaderUniforms(Scene &scene, bool lightChanged);
    void updateCascadeShadowUBO(const Cascades &cascades, bool lightChanged, bool uploadAll);
};

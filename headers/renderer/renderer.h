#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "buffers.h"
#include "cascade_shadow.h"
#include "compute_shader.h"
#include "gl_state.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "graphics_settings.h"
#include "graphics_shader.h"
#include "handle.h"
#include "render_target.h"
#include "renderer_types.h"
#include "scene.h"
#include "scene_processor.h"
#include "shader_bucket_hash.h"
#include "texture_manager.h"
#include "thread_pool.h"

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
    GlobalVAO VAO_;

    // Textures
    CascadeShadow cascadeShadow_;
    RenderTarget msaaShadowTarget_;
    RenderTarget shadowPassTarget_;

    // Drawing
    std::vector<MultiDrawBucket> drawBuckets_;

    // Compute Shaders
    ComputeShader cullShader_;
    ComputeShader compactionShader_;
    ComputeShader particleShader_;
    std::uint32_t cullWorkGroupSize_{0};
    std::uint32_t compactionWorkGroupSize_{0};
    std::vector<ParticleUniforms> particleUniforms_;

    // Initialization
    void loadShaders();
    void setupShader(GraphicsShader &shader);

    // Setup
    void setSceneShaderUniforms(Scene &scene);
    void takeBufferData(SceneProcessor::ProcessedScene &processed);
    void takeGPUObjects(SceneProcessor::ProcessedScene &processed);
    void setLocalGroupSizes(SceneProcessor::ProcessedScene &processed, std::size_t numDrawCommands);
    void setParticleUniforms(
        const std::unordered_map<ShaderBucket, SceneProcessor::BucketData, ShaderBucketHash> &bucketMap,
        const std::vector<std::size_t> &vertexCounts);

    // Cubemap Generation
    void borrowSkybox(const std::unordered_map<ShaderBucket, SceneProcessor::BucketData, ShaderBucketHash> &bucketMap);
    void createCubemaps(Scene &scene,
                        const std::unordered_map<ShaderBucket, SceneProcessor::BucketData, ShaderBucketHash> &bucketMap,
                        const std::vector<DrawElementsIndirectCommand> &commandsSource);
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

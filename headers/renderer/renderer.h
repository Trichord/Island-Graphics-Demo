#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aabb.h"
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

    void renderScene(Scene &scene);
    void initializeResources(Scene &scene);

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
    std::uint32_t cullWorkGroupSize_{0};
    std::uint32_t compactionWorkGroupSize_{0};

    // Shaders and Uniforms
    void loadShaders();
    void setupShader(GraphicsShader &shader);
    void setSceneShaderUniforms(Scene &scene);

    // Setup
    void takeBuffers(SceneProcessor::ProcessedScene &processed);

    // Cubemap Generation
    void borrowSkybox(const SceneProcessor &processor);
    void createCubemaps(Scene &scene, const SceneProcessor &processor,
                        std::vector<DrawElementsIndirectCommand> &commands);
    void renderCubeFaces(Scene &scene, RenderTarget &target, RenderTarget &blitTarget);

    // Drawing
    void renderScene();
    void cullScene(const glm::mat4 &cullMatrix);
    void renderShadowMaps(Scene &scene, bool lightChanged);
    void shadowPass(GraphicsShader &shader, int cascade, bool alphaTested);

    // Updates
    void updateCameraUBO(Scene &scene);
    void updateSceneTransforms(Scene &scene);
    void updateFrustumPlanesUBO(const glm::mat4 &cullMatrix);
    void updateShaderUniforms(Scene &scene, bool lightChanged);
    void updateCascadeShadowUBO(const Cascades &cascades, bool lightChanged, bool uploadAll);
};

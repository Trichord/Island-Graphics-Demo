#pragma once

#include <array>
#include <cstdint>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <set>
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
#include "renderer/texture_view.h"
#include "scene/camera.h"
#include "scene/handle.h"
#include "scene/scene.h"
#include "utilities/scene_processor.h"
#include "utilities/texture_manager.h"

class Renderer
{
  public:
    Renderer(TextureManager &textureManager, const GraphicsSettings &settings, int screenWidth, int screenHeight);
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    Renderer(Renderer &&other) = delete;
    Renderer &operator=(Renderer &&other) = delete;
    ~Renderer();

    void initializeResources(const Scene &scene, const Camera &camera);
    void renderScene(const Scene &scene, const Camera &camera, float deltaTime);
    void updateExternalResolution(int width, int height);

    std::uint32_t getFrameCount() const;

  private:
    static constexpr int hiZResolutionReduction{2};

    const GraphicsSettings graphicsSettings_;
    TextureManager &textureManager_;
    int externalWidth_{1920};
    int externalHeight_{1080};
    std::uint32_t framesRendered_{0};

    RenderTarget frameBuffer_;
    RenderTarget msaaResolveTarget_;
    RenderTarget shadowPassTarget_;

    GLState glState_;
    std::array<GraphicsShader, static_cast<int>(ShaderType::count)> shaders_;

    // GPU Resources
    VAOs VAOs_;
    UBOs UBOs_;
    SSBOs SSBOs_;
    GLuint hiZTexture_{0};
    TextureView hiZViews_;

    // Scene Updates
    std::vector<glm::mat4> updatedMatrices_;
    std::vector<std::vector<std::uint32_t>> matricesBaseIndices_;
    std::unordered_map<Handle, std::uint32_t> boneTransformsBaseIndexMap_;

    // Shadows
    CascadeShadow cascadeShadow_;

    // Drawing
    std::vector<MultiDrawBucket> drawBuckets_;

    // Compute Shaders
    ComputeShader cullShader_;
    ComputeShader cullFrustumShader_;
    ComputeShader compactionShader_;
    ComputeShader particleShader_;
    ComputeShader skinningShader_;
    ComputeShader hiZShader_;
    ComputeShader msaaDepthResolveShader_;

    std::uint32_t cullWorkGroupSize_{0};
    std::uint32_t compactionWorkGroupSize_{0};
    std::uint32_t particleWorkGroupSize_{0};
    std::uint32_t skinnedVertexGroupSize_{0};

    // SSBO sizes for rebinding buffer ranges.
    std::uint32_t drawCommandsSize_{0};
    std::uint32_t cullResultsSize_{0};
    std::uint32_t drawCountsSize_{0};
    std::uint32_t visibleInstancesSize_{0};

    // Initialization
    void loadShaders();
    void setupShader(GraphicsShader &shader);
    GLuint createResolveTexture();

    // Setup
    void setSceneShaderUniforms(const Scene &scene, const Camera &camera);
    void takeBufferData(SceneProcessor::ProcessedScene &processed);
    void takeGPUObjects(SceneProcessor::ProcessedScene &processed);
    void setupComputeShaders(const SceneProcessor::ProcessedScene &processed, float nearPlane);

    // Cubemap Generation
    void borrowSkybox(const std::vector<MultiDrawBucket> &drawBuckets,
                      const std::vector<std::set<Handle>> &bucketHandles);
    void createCubemaps(const Scene &scene, const std::vector<MultiDrawBucket> &drawBuckets);
    void renderCubeFaces(Camera &camera, RenderTarget &target, RenderTarget &blitTarget,
                         std::size_t parameterBufferIndex);
    void updateMetadataCubemaps(const Scene &scene, const std::vector<std::uint32_t> &metadataBridge);

    // Drawing
    void blitFrame();
    void renderScene();
    void bindBufferRanges(int index);
    void shadowPass(GraphicsShader &shader, int cascade, bool alphaTested);
    void renderShadowMaps(const Scene &scene, const Camera &camera, bool lightChanged);
    void cullScene(ComputeShader &cullShader, const glm::mat4 &cullMatrix, int bufferSection);

    // Updates
    void updateHiZ();
    void updateParticles();
    void updateSkinnedVertices();
    void updateTimeUBO(float deltaTime);
    void updateCameraUBO(const Camera &camera);
    void updateSceneTransforms(const Scene &scene);
    void updateFrustumPlanesUBO(const glm::mat4 &cullMatrix);
    void updateShaderUniforms(const Scene &scene, bool lightChanged);
    void updateCascades(const Scene &scene, const Camera &camera, bool lightChanged);
    void updateCascadeShadowUBO(const Cascades &cascades, bool lightChanged, bool uploadAll);
};

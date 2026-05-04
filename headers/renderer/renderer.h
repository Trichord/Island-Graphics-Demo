#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cascade_shadow.h"
#include "gl_state.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "graphics_settings.h"
#include "graphics_shader.h"
#include "render_target.h"
#include "renderer_types.h"
#include "scene.h"
#include "texture_manager.h"
#include "textures.h"
#include "thread_pool.h"

class Renderer
{
  public:
    Renderer() = delete;
    explicit Renderer(const GraphicsSettings &graphicsSettings);
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    Renderer(Renderer &&other) = delete;
    Renderer &operator=(Renderer &&other) = delete;
    ~Renderer();

    void renderScene(Scene &scene);
    void initializeResources(Scene &scene);
    int getReflectionResolution() const;

    std::uint32_t getFrameCount() const;
    TextureManager &getTextureManager();

  private:
    std::uint32_t framesRendered_{0};
    static constexpr float depthClear_{1.0f};
    static constexpr std::array<float, 4> clearColor_{0.0f, 0.0f, 0.0f, 1.0f};

    // Critical members
    const GraphicsSettings graphicsSettings_;
    ThreadPool threadPool_;
    TextureManager textureManager_;

    // Owned objects
    GLState glState_;
    std::vector<glm::mat4> updatedMatrices_;
    std::unordered_map<ModelHandle, ModelTransforms> transformsByHandle_;
    std::array<GraphicsShader, static_cast<int>(ShaderType::count)> shaders_;

    // Global VAO
    GLuint globalVAO_{0};
    GLuint globalVBO_{0};
    GLuint globalEBO_{0};

    // UBO
    GLuint shaderUniformsUBO_{0};
    GLuint shadowCascadesUBO_{0};

    /* Buffers
     *
     * AABB: Per gLTF primitive AABBs
     * Matrices: Per Model transforms (multiple primitives share)
     * Metadata: Per Primitive materials, buffer offsets
     * DrawCommands: Per primitive, updated every frame
     * DrawCommandsSource: Per primitive. Set once during loading
     * VisibleInstances: Indexes into the matrices buffer. Each primitive gets its own section, sized to the number
     *                   of model instances. Used for culling.
     *
     * AABBs, Metadata, DrawCommands, and DrawCommandsSource are all mapped 1:1.
     * i.e. Metadata[i] gives texture info for DrawCommands[i]
     *
     */

    // Buffers
    GLuint aabbBuffer_{0};
    GLuint matricesBuffer_{0};
    GLuint metadataBuffer_{0};
    GLuint drawCommandsBuffer_{0};
    GLuint visibleInstancesBuffer_{0};
    GLuint drawCommandsSourceBuffer_{0};

    // Textures
    CascadeShadow cascadeShadow_;
    RenderTarget msaaShadowTarget_;
    RenderTarget shadowPassTarget_;

    // Drawing
    DrawElementsIndirectCommand skybox_;
    std::vector<MultiDrawBucket> drawBuckets_;
    std::vector<DrawElementsIndirectCommand> drawCommands_;
    std::vector<DrawElementsIndirectCommand> drawCommandsSource_;

    // Culling (currently simulating SSBOs)
    std::vector<AABB> aabbs_;
    std::vector<glm::mat4> matrices_;
    std::vector<std::uint32_t> visibleInstances_;
    std::vector<PrimitiveMetadata> metadata_;
    std::vector<BufferMap> bufferMap_;
    std::vector<std::uint32_t> drawCounts_;        // atomic MDI Count buffer
    std::vector<std::uint32_t> cullResults_; // per draw command visibility result

    // Shaders and Uniforms
    void loadShaders();
    void setupShader(GraphicsShader &shader);
    void setSceneShaderUniforms(Scene &scene);

    // Cubemap Generation
    void borrowSkybox(const std::vector<DrawEntry> &entries);
    void renderCubeFaces(Scene &scene, RenderTarget &target, RenderTarget &blitTarget);
    void createCubemaps(Scene &scene, const std::vector<DrawEntry> &entries, SamplerArrayEntry reservedEntry);
    void updateMetadataCubemaps(const std::vector<DrawEntry> &entries, std::vector<PrimitiveMetadata> &metadata);

    // GPU buffers
    void createBuffers(const SceneTotals &totals);
    void createVAO(const std::vector<DrawEntry> &entries, const SceneTotals &totals);

    // Metadata Setup
    void setMetadataMaterials(const std::vector<DrawEntry> &entries, std::vector<PrimitiveMetadata> &metadata);
    void populateMatricesSSBO(const std::vector<DrawEntry> &entries, std::vector<PrimitiveMetadata> &metadata,
                              const SceneTotals &totals);
    void setVisibleInstancesBaseIndex(const std::vector<DrawEntry> &entries, std::vector<PrimitiveMetadata> &metadata,
                                      const SceneTotals &totals);

    // Renderer internal setup
    void sortDrawEntries(std::vector<DrawEntry> &entries);
    void createDrawBuckets(const std::vector<DrawEntry> &entries);
    std::vector<DrawEntry> createDrawEntries(Scene &scene, const SceneTotals &totals);
    void createAABBs(const std::vector<DrawEntry> &entries, const SceneTotals &totals);
    void createBufferMap(const std::vector<DrawEntry> &entries, const SceneTotals &totals);

    // Drawing
    void renderScene();
    void renderShadowMaps(Scene &scene, bool lightChanged);
    void shadowPass(GraphicsShader &shader, int cascade, bool alphaTested);

    // Culling
    void compactBuffer();
    void cullScene(const glm::mat4 &cullMatrix);
    void cullVisibilityIndex(std::uint32_t baseVisibilityIndex, std::uint32_t visibilityIndex, std::uint32_t &counter,
                             const std::array<glm::vec4, 6> &frustumPlanes);

    // Updates
    void updateCameraUBO(Scene &scene);
    void updateSceneTransforms(Scene &scene);
    void updateShaderUniforms(Scene &scene, bool lightChanged);
    void updateCascadeShadowUBO(const Cascades &cascades, bool lightChanged, bool uploadAll);

    // Cleanup
    void deleteBuffers();
};

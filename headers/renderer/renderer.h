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

    // UBO
    GLuint shaderUniformsUBO_{0};
    GLuint shadowCascadesUBO_{0};
    PackedShaderUniforms packedShaderUniforms_;
    PackedCascadeShadows packedCascadeShadows_;

    /* SSBOs
     *
     * Renderer uploads a static-sized SSBO containing every renderable's model matrix. An visible indices SSBO capable
     * of holding the same number of elements is reserved in GPU memory. A fresh vector of indices is computed and
     * uploaded every frame, representing the visible indices in the static buffer. The shader starts at model offset
     * and indices offset, and iterates through its indices.
     *
     * e.g.
     *
     *                  Matrices
     *
     *  | Model 1   | Model 2            | M3  | M4  |
     * [0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 0, 1, 0, 1, 2]
     *
     *                  Visible Indices
     *  | M1  | M2   |  (M3 and M4 not visible this frame)
     * [2, 3, 0, 5, 6]
     *
     */

    // Buffers
    GLuint matricesSSBO_{0};
    GLuint metadataSSBO_{0};
    GLuint visibleIndicesSSBO_{0};
    GLuint drawCommandsBuffer_{0};

    // Owned objects
    GLState glState_;
    std::vector<std::uint32_t> visibleMatrices_;
    std::array<GraphicsShader, static_cast<int>(ShaderType::count)> shaders_;
    std::unordered_map<ModelHandle, std::vector<std::uint32_t>> drawEntriesByHandle_;
    std::vector<glm::mat4> updatedMatrices_;

    // Global VAO
    GLuint globalVAO_{0};
    GLuint globalVBO_{0};
    GLuint globalEBO_{0};

    // Textures
    CascadeShadow cascadeShadow_;
    RenderTarget msaaShadowTarget_;
    RenderTarget shadowPassTarget_;

    // Drawing
    std::vector<DrawEntry> drawEntries_;
    std::vector<MultiDrawInfo> drawBuckets_;
    std::vector<DrawElementsIndirectCommand> drawCommands_;

    // Shaders and Uniforms
    void loadShaders();
    void setupShader(GraphicsShader &shader);
    void setSceneShaderUniforms(Scene &scene);

    // Cubemap Generation
    void borrowSkybox();
    void createCubemaps(Scene &scene, SamplerArrayEntry reservedEntry);
    void updateMetadataCubemaps(std::vector<PrimitiveMetadata> &metadata);
    void renderCubeFaces(Scene &scene, RenderTarget &target, RenderTarget &blitTarget);

    // GPU buffers
    void assignMetadataIndices();
    void createVAO(Scene &scene, SceneTotals stats);
    void populateMetadataMaterials(Scene &scene, std::vector<PrimitiveMetadata> &metadata);
    void createBuffers(std::vector<PrimitiveMetadata> &metadata, const SceneTotals &totals);
    void populateMatricesSSBO(Scene &scene, std::vector<PrimitiveMetadata> &metadata, SceneTotals &stats);
    void populateVisibleIndicesBuffer(std::vector<PrimitiveMetadata> &metadata, SceneTotals &stats);

    // Counting
    SceneTotals getSceneTotals(Scene &scene);

    // Renderer Draw List setup
    void sortPrimitives();
    void buildDrawEntries(Scene &scene);

    // Drawing
    void renderScene();
    void shadowPass(GraphicsShader &shader, int cascade, bool alphaTested);
    void renderShadowMaps(Scene &scene, bool lightChanged);

    // Culling
    void cullNone(DrawEntry &renderable);
    void cullScene(Scene &scene, const glm::mat4 &cullMatrix);
    void cullDrawEntry(DrawEntry &renderable, const std::array<glm::vec4, 6> &frustumPlanes);

    // Updates
    void updateCameraUBO(Scene &scene);
    void updateSceneTransforms(Scene &scene);
    void updateDrawCommandBuffer(bool upload = true);
    void updateShaderUniforms(Scene &scene, bool lightChanged);
    void updateCascadeShadowUBO(const Cascades &cascades, bool lightChanged, bool uploadAll);

    // Cleanup
    void deleteBuffers();
};

#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "cascade_shadow.h"
#include "compute_shader.h"
#include "gl_state.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "graphics_settings.h"
#include "graphics_shader.h"
#include "render_target.h"
#include "renderer_types.h"
#include "scene.h"
#include "setup_functions.h"
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
    std::unordered_map<Handle, BufferTransforms> transformsByHandle_;
    std::array<GraphicsShader, static_cast<int>(ShaderType::count)> shaders_;

    /* Buffers
     *
     * Overall strategy: maintain set regions for each primitive so that a base index is always known. From there, any
     * instancing is always base + instance.
     *
     * Map: Per (primitive × instance) pair. Allows any index in the VisibleInstances buffer to see its matching:
     *      - matricesIndex: index into Matrices buffer
     *      - sharedBaseIndex: index into AABBs/Metadata/DrawCommands (the primitive)
     *      - drawBucket: which draw bucket this primitive belongs to
     *      - drawBucketBaseIndex: start of that bucket in DrawCommands
     *      - visibleInstancesBaseIndex: start of the (primitive x instance) in the visibility buffer
     *
     * AABB: Per primitive. Local-space bounding boxes
     * Matrices: Per model. World-space transforms (multiple primitives share one range)
     * Metadata: Per primitive. Material textures, visibility buffer base index
     * DrawCounts: Per draw bucket. Number of surviving draw commands after compaction
     * CullResults: Per primitive. Number of surviving instances after frustum test
     * DrawCommands: Per primitive. Compacted each frame from DrawCommandsSource
     * VisibleInstances: Per (primitive × instance) pair. Indices into Matrices for
     *                   instances that passed the frustum test. Each primitive owns a
     *                   contiguous section sized to its instance count.
     * DrawCommandsSource: Per primitive. Immutable source of truth, set once at load
     *
     * 1:1 indexed buffers: AABB[i], Metadata[i], DrawCommands[i], DrawCommandsSource[i]
     * Flattened buffers: Map[j], VisibleInstances[j] where j spans all (primitive × instance) pairs
     *
     */

    // Buffers
    GLuint mapBuffer_{0};
    GLuint aabbBuffer_{0};
    GLuint matricesBuffer_{0};
    GLuint metadataBuffer_{0};
    GLuint drawCountsBuffer_{0};
    GLuint cullResultsBuffer_{0};
    GLuint drawCommandsBuffer_{0};
    GLuint visibleInstancesBuffer_{0};
    GLuint drawCommandsSourceBuffer_{0};

    // Global VAO
    GLuint globalVAO_{0};
    GLuint globalVBO_{0};
    GLuint globalEBO_{0};

    // UBO
    GLuint frustumPlanesUBO_{0};
    GLuint shaderUniformsUBO_{0};
    GLuint shadowCascadesUBO_{0};

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

    // Cubemap Generation
    void borrowSkybox(const std::vector<DrawEntry> &entries);
    void renderCubeFaces(Scene &scene, RenderTarget &target, RenderTarget &blitTarget);
    void createCubemaps(Scene &scene, const std::vector<DrawElementsIndirectCommand> &commandsSource,
                        const std::vector<DrawEntry> &entries, SamplerArrayEntry reservedEntry);
    void updateMetadataCubemaps(const std::vector<DrawEntry> &entries, std::vector<PrimitiveMetadata> &metadata);

    // GPU buffers
    void createVAO(const std::vector<DrawEntry> &entries, const SceneTotals &totals);
    void createBuffers(const std::vector<DrawElementsIndirectCommand> &commands, const std::vector<AABB> &aabbs,
                       const SceneTotals &totals);
    void populateMatricesSSBO(const std::vector<DrawEntry> &entries, std::vector<PrimitiveMetadata> &metadata,
                              const SceneTotals &totals);

    // Renderer internal setup
    void createBufferMap(const std::vector<DrawEntry> &entries, const std::vector<PrimitiveMetadata> &metadata,
                         const SceneTotals &totals);

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

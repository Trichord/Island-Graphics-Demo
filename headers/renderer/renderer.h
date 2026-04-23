#pragma once

#include <array>
#include <unordered_map>
#include <vector>

#include "aabb.h"
#include "cascade_shadow.h"
#include "fastgltf/types.hpp"
#include "gl_state.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "graphics_settings.h"
#include "render_target.h"
#include "scene.h"
#include "shader.h"
#include "shader_type.h"
#include "texture_manager.h"
#include "thread_pool.h"
#include "transform.h"
#include <cstdint>
#include <primitive.h>

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

    std::uint32_t getFrameCount() const;
    TextureManager &getTextureManager();

  private:
    static constexpr float depthClear_{1.0f};
    static constexpr std::array<float, 4> clearColor_{0.0f, 0.0f, 0.0f, 1.0f};

    struct DrawEntry
    {
        ModelHandle handle;
        Primitive *primitive;
        std::vector<Transform> *transforms;

        ShaderType shaderType{ShaderType::directionalPBR};
        fastgltf::AlphaMode alphaMode{fastgltf::AlphaMode::Opaque};

        bool anyVisible{true};
        std::uint32_t visibleInstances{0};

        std::uint32_t vaoBaseIndex{0};
        std::uint32_t eboBaseIndex{0};
        std::uint32_t metadataBufferIndex{0};
        std::uint32_t matricesBufferBaseIndex{0};
        std::uint32_t visibleIndicesBufferBaseIndex{0};
    };

    // Packed UBO data holds everything we want to share with all shaders
    struct PackedShaderUniforms
    {
        glm::mat4 viewMatrix{1.0f};
        glm::mat4 projectionMatrix{1.0f};
        glm::mat4 viewProjectionMatrix{1.0f};
        glm::mat4 inverseViewProjectMatrix{1.0f};
        glm::vec4 cameraPosition{0.0f};
        float time{0.0f};
    };

    struct PackedShadowCascadeData
    {
        glm::mat4 matrices[CascadeShadow::cascadeCount];
        glm::vec4 splits;
    };

    // Allows primitives to read all their data from the SSBO
    struct PrimitiveData
    {
        int albedoArray{0};
        int albedoLayer{0};
        int armArray{0};
        int armLayer{0};
        int normalArray{0};
        int normalLayer{0};
        int matricesBufferBaseIndex{0};
    };

    // Keep track of the number of vertices and indices in an entire scene
    struct SceneTotals
    {
        std::size_t vertices{0};
        std::size_t indices{0};
        std::size_t matrices{0};
        std::size_t visibleInstances{0};
    };

    std::uint32_t framesRendered_{0};
    ThreadPool threadPool_;

    // UBO
    GLuint cameraUBO_{0};
    GLuint shadowCascadesUBO_{0};
    PackedShaderUniforms packedShaderUniforms_;
    PackedShadowCascadeData packedShadowCascadeData_;

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

    GLuint matricesSSBO_{0};
    GLuint primitivesSSBO_{0};
    GLuint visibleIndicesSSBO_{0};

    // Owned objects
    GLState glState_;
    TextureManager textureManager_;
    std::vector<std::uint32_t> visibleMatrices_;
    std::unordered_map<ShaderType, Shader> shaders_;
    const GraphicsSettings graphicsSettings_;
    std::unordered_map<ModelHandle, std::vector<std::uint32_t>> drawEntriesByHandle_;
    std::vector<glm::mat4> updatedMatrices_;

    // Global VAO
    GLuint globalVAO_{0};
    GLuint globalVBO_{0};
    GLuint globalEBO_{0};

    // Textures
    CascadeShadow cascadeShadow_;
    RenderTarget shadowPassTarget_;
    RenderTarget shadowBlitTarget_;

    // Scene objects for drawing
    std::vector<DrawEntry> drawEntries_;

    // Renderer initialization
    void loadShaders();

    // Scene dependent initialization
    void borrowSkybox();
    void sortPrimitives();
    void createBuffers(Scene &scene);
    void buildDrawEntries(Scene &scene);
    void createCubemaps(Scene &scene);
    void setSceneShaderUniforms(Scene &scene);
    void createVAO(Scene &scene, SceneTotals stats);
    void populateMetadataSSBO(Scene &scene, SceneTotals &stats);
    void populateMatricesSSBO(Scene &scene, SceneTotals &stats);
    void createCubemapFaces(Scene &scene, int renderTargetSize, GLuint texture);

    // Scene independant initialization
    void setupShader(Shader &shader);

    // Rendering
    void renderAllSceneObjects();
    void renderObject(Shader &shader, DrawEntry &renderable);
    void shadowPass(const CascadeShadow::LightMatrices &lightMatrices, Shader &shader, int cascade,
                    bool drawAlphaTested);

    // Culling
    void resetVisibility();
    void cullNone(DrawEntry &renderable, int &currentOffset);
    void cullAllModels(Scene &scene, const glm::mat4 &cullMatrix);
    void cullInstanced(DrawEntry &renderable, const std::array<glm::vec4, 6> &frustumPlanes, int &currentOffset);

    // Updates
    void updateCameraUBO(Scene &scene);
    void updateShadowMaps(Scene &scene);
    void updateSceneTransforms(Scene &scene);
    void updateShaderUniforms(Scene &scene, bool lightDirectionChanged);
    void updateCascadeShadowUBO(const CascadeShadow::CascadeSplits &cascades,
                                const CascadeShadow::LightMatrices &matrices);

    // Cleanup
    void deleteBuffers();
};

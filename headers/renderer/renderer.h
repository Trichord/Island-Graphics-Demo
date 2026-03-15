#pragma once

#include <array>
#include <unordered_map>
#include <vector>

#include "config_window.h"
#include "gl_state.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "render_target.h"
#include "renderable.h"
#include "scene.h"
#include "shader.h"
#include "shader_type.h"
#include "texture_manager.h"
#include "thread_pool.h"

// Desired threadcount for the texture loading threadpool
constexpr unsigned int threadCount{6u};
constexpr int shadowMapRes{2048};

class Renderer
{
  public:
    Renderer() = delete;
    explicit Renderer(const ConfigWindow::GraphicsSettings &graphicsSettings);
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    Renderer(Renderer &&other) = delete;
    Renderer &operator=(Renderer &&other) = delete;
    ~Renderer();

    void renderScene(Scene &scene);
    void updateUBO(Scene &scene);
    void initializeResources(Scene &scene);
    void allResourcesLoaded();
    TextureManager &getTextureManager();

  private:
    struct ssboOffsets
    {
        GLuint matricesOffset{0};
        GLuint indicesOffset{0};
    };

    static constexpr float depthClear_{1.0f};
    static constexpr std::array<float, 4> clearColor_{0.0f, 0.0f, 0.0f, 1.0f};

    // Packed UBO data holds everything we want to share with all shaders
    struct PackedCameraData
    {
        glm::mat4 viewMatrix{1.0f};
        glm::mat4 projectionMatrix{1.0f};
        glm::mat4 viewProjectionMatrix{1.0f};
        glm::mat4 inverseViewProjectMatrix{1.0f};
        glm::vec4 cameraPosition{0.0f};
    };

    ThreadPool threadPool_{threadCount};

    // UBO
    GLuint UBO_{0};
    PackedCameraData uboCameraData_;

    /* SSBOs
     *
     * Renderer uploads a static SSBO containing every renderable's model matrix. An indices SSBO capable of
     * holding the same number of elements is reserved in GPU memory. A fresh vector of indices is computed and uploaded
     * every frame, representing the visible indices in the static buffer.
     *
     * e.g.
     *
     *                  Matrices
     *
     *  | Model 1   | Model 2            | M3  | M4  |
     * [0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 0, 1, 0, 1, 2]
     *
     *                  Indices
     *  | M1  | M2   |  (M3 and M4 not visible this frame)
     * [2, 3, 0, 5, 6]
     *
     */
    GLuint indicesSSBO_{0};
    GLuint matricesSSBO_{0};
    std::unordered_map<ModelHandle, ssboOffsets> modelBaseIndices_;

    // Owned objects
    GLState glState_;
    TextureManager textureManager_;
    std::vector<GLuint> instanceIndices_;
    std::unordered_map<ShaderType, Shader> shaders_;
    const ConfigWindow::GraphicsSettings graphicsSettings_;

    // Textures
    RenderTarget shadowPassTarget_;
    RenderTarget shadowBlitTarget_;

    // Shaders
    Shader shadowPassShader_;

    // Renderer initialization
    void loadShaders();

    // Scene dependent initialization
    void createBuffers(Scene &scene);
    void setupShader(Shader &shader);
    void borrowSkyboxes(Scene &scene);
    void createCubemaps(Scene &scene);
    void createShadowMaps(Scene &scene);
    void loadInstancedData(Scene &scene);
    void setSceneShaderUniforms(Scene &scene);
    void populateSSBOs(Scene &scene, std::size_t totalInstanceCount);
    void createCubemapFaces(Scene &scene, RenderTarget &msaaTarget, RenderTarget &blitTarget, GLuint texture);

    // Rendering
    void updateShadowMaps(Scene &scene);
    void updateSceneTransforms(Scene &scene);
    void renderAllSceneObjects(Scene &scene);
    void renderObject(Renderable &renderable);
    void bindTextures(const Material &materials);
    void updateShaderUniforms(Scene &scene, bool lightDirectionChanged);

    // Culling
    void resetVisibility(Scene &scene);
    void cullRenderables(Scene &scene);
    void cullNone(Renderable &renderable, GLuint &currentOffset);
    void cullByMesh(Renderable &renderable, const std::array<glm::vec4, 6> &frustumPlanes, GLuint &currentOffset);
    void cullInstanced(Renderable &renderable, const glm::vec3 &cameraPos,
                       const std::array<glm::vec4, 6> &frustumPlanes, GLuint &currentOffset);

    // Cleanup
    void deleteBuffers();
};

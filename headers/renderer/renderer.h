#pragma once

#include <array>
#include <unordered_map>
#include <vector>

#include "config_window.h"
#include "gl_state.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "renderable.h"
#include "scene.h"
#include "shader.h"
#include "shader_type.h"
#include "texture_manager.h"
#include "thread_pool.h"

// Desired threadcount for the texture loading threadpool
constexpr unsigned int threadCount{6u};

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
    std::uint32_t indicesOffset_{0};
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

    // SSBOs
    GLuint indicesSSBO_{0};
    GLuint matricesSSBO_{0};
    std::unordered_map<ModelHandle, GLuint> instancedBaseOffsets_;

    // Owned objects
    GLState glState_;
    TextureManager textureManager_;
    std::vector<std::uint32_t> instanceIndices_;
    std::unordered_map<ShaderType, Shader> shaders_;
    const ConfigWindow::GraphicsSettings graphicsSettings_;

    // Renderer initialization
    void loadShaders();
    void setupTextureManager();

    // Scene initialization
    void createBuffers(Scene &scene);
    void setupShader(Shader &shader);
    void borrowSkyboxes(Scene &scene);
    void generateCubeMaps(Scene &scene);
    void loadInstancedData(Scene &scene);
    void populateSSBOs(Scene &scene, std::size_t totalInstanceCount);

    // Rendering
    void updateShaderUniforms(Scene &scene);
    void renderAllSceneObjects(Scene &scene);
    void renderObject(Renderable &renderable);
    void bindTextures(const Material &materials);

    // Culling
    void resetVisibility(Scene &scene);
    void cullRenderables(Scene &scene);
    void cullModel(Renderable &renderable, const std::array<glm::vec4, 6> &frustumPlanes);
    void cullByMesh(Renderable &renderable, const std::array<glm::vec4, 6> &frustumPlanes);
    void cullInstanced(Renderable &renderable, const glm::vec3 &cameraPos,
                       const std::array<glm::vec4, 6> &frustumPlanes);

    // Cleanup
    void deleteBuffers();
};

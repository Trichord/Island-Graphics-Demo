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
#include "shader_types.h"
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
    static constexpr float depthClear{1.0f};
    static constexpr std::array<float, 4> clearColor{0.0f, 0.0f, 0.0f, 1.0f};

    // Packed UBO data holds everything we want to share with all shaders
    struct PackedCameraData
    {
        glm::mat4 viewMatrix{1.0f};
        glm::mat4 projectionMatrix{1.0f};
        glm::mat4 viewProjectionMatrix{1.0f};
        glm::mat4 inverseViewProjectMatrix{1.0f};
        glm::vec4 cameraPosition{0.0f};
    };

    ThreadPool threadPool{threadCount};

    // UBO
    GLuint UBO{0};
    PackedCameraData uboCameraData;

    // SSBOs
    GLuint indicesSSBO{0};
    GLuint matricesSSBO{0};  
    std::unordered_map<ModelHandle, GLuint> instancedBaseOffsets;

    // Owned objects
    GLState glState;
    TextureManager textureManager;
    std::vector<std::uint32_t> instanceIndices;
    std::unordered_map<ShaderType, Shader> shaders;
    const ConfigWindow::GraphicsSettings graphicsSettings;

    void loadShaders();
    void setupTextureManager();
    void updateSSBO(Scene &scene);
    void setupShader(Shader &shader);
    void resetVisibility(Scene &scene);
    void updateShaderUniforms(Scene &scene);
    void bindTextures(const Material &materials);
    void cullRenderables(Scene &scene);
    void cullInstanced(Renderable &renderable, const glm::vec3 &cameraPos,
                       const std::array<glm::vec4, 6> &frustumPlanes);
    void cullModel(Renderable &renderable, const std::array<glm::vec4, 6> &frustumPlanes);
    void cullByMesh(Renderable &renderable, const std::array<glm::vec4, 6> &frustumPlanes);
    void renderAllSceneObjects(Scene &scene);
    void renderObject(Renderable &renderable);
    void generateCubeMaps(Scene &scene);
    void createBuffers(Scene &scene);
    void borrowSkyboxes(Scene &scene);
    void populateSSBO(Scene &scene, std::size_t totalInstanceCount);
    void deleteBuffers();
};

#pragma once

#include <array>
#include <unordered_map>

#include "config_window.h"
#include "gl_state.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "scene.h"
#include "shader.h"
#include "shader_types.h"
#include "texture_manager.h"

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
    struct PackedUBOData
    {
        glm::mat4 viewMatrix{1.0f};
        glm::mat4 projectionMatrix{1.0f};
        glm::mat4 viewProjectionMatrix{1.0f};
        glm::mat4 inverseViewProjectMatrix{1.0f};
        glm::vec4 cameraPosition{0.0f};
    };

    struct SSBOData
    {
        glm::mat4 modelMatrix{1.0f};
        glm::mat4 normalMatrix{1.0f};
    };

    ThreadPool threadPool{threadCount};

    // UBO
    GLuint UBO{0};
    PackedUBOData uboData;

    // SSBO
    GLuint SSBO{0};
    std::vector<SSBOData> ssboData;
    std::unordered_map<ModelHandle, GLuint> ssboOffsets;

    // Owned objects
    GLState glState;
    TextureManager textureManager;
    std::unordered_map<ShaderType, Shader> shaders;
    const ConfigWindow::GraphicsSettings graphicsSettings;

    void loadShaders();
    void setupTextureManager();
    void setupShader(Shader &shader);
    void resetVisibility(Scene &scene);
    void renderSkybox(Scene &scene);
    void updateShaderUniforms(Scene &scene);
    void bindTextures(const Material &materials);
    void cullRenderables(Scene &scene);
    void cullInstanced(Renderable &renderable, const glm::vec3 &cameraPos,
                       const std::array<glm::vec4, 6> &frustumPlanes, bool cullByDistance);
    void cullModel(Renderable &renderable, const std::array<glm::vec4, 6> &frustumPlanes);
    void cullByMesh(Renderable &renderable, const std::array<glm::vec4, 6> &frustumPlanes);
    void renderAllSceneObjects(Scene &scene);
    void renderObject(IRenderable &object);
    void generateCubeMaps(Scene &scene);
    void updateSSBO(IRenderable &object);
    void createBuffers(Scene &scene);
    void borrowSkyboxes(Scene &scene);
    void populateSSBO(Scene &scene);
    void deleteBuffers();
};

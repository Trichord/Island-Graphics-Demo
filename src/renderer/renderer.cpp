#include "renderer.h"

#include <array>
#include <cstring>
#include <iostream>
#include <unordered_map>

#include "camera.h"
#include "config_window.h"
#include "directional_light.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "renderable.h"
#include "scene.h"
#include "shader.h"
#include "shader_types.h"
#include "transform.h"

// Normalized wind direction, east southeast with a slight downward angle
constexpr glm::vec3 windDir{0.892079f, -0.101373f, 0.440362f};
constexpr glm::vec3 ambientLight{0.15f};
// Bright midday sun
constexpr glm::vec3 lightColor{1.0f, 0.95f, 0.9f};

Renderer::Renderer(const ConfigWindow::GraphicsSettings& settings)
    : graphicsSettings{settings} {
  setupUBO();

  std::cout << "Loading Shaders...";
  loadShaders();
  std::cout << " Done." << std::endl;

  for (auto& [_, shader] : shaders) setupShader(shader);
}

Renderer::Renderer(Renderer&& other) noexcept
    : graphicsSettings{other.graphicsSettings},
      shaders{std::move(other.shaders)},
      uboData{std::move(other.uboData)},
      UBO{other.UBO} {
  other.UBO = 0;
}

void Renderer::renderScene(Scene& scene) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  updateShaderUniforms(scene);
  cullRenderables(scene);

  // For each shader type in the scene
  for (auto& [shaderType, objectList] : scene.getSceneObjects()) {
    Shader& shader{shaders.at(shaderType)};
    shader.use();

    // For each object of that shader type
    for (auto& object : objectList) {
      if (!object.isVisible) continue;

      glm::mat4 modelMatrix{object.transform.getModelMatrix()};
      glm::mat3 normalMatrix{object.transform.getNormalMatrix()};

      switch (shaderType) {
        case ShaderType::DIRECTIONAL_PBR:
        case ShaderType::POINT_PBR:
        case ShaderType::REFLECTIVE:
          shader.setMat4("modelMatrix", modelMatrix);
          shader.setMat3("normalMatrix", normalMatrix);
          break;

        case ShaderType::LIGHT_SOURCE:
          shader.setMat4("modelMatrix", modelMatrix);
          break;
      }

      // Draw it
      object.renderable.get().render(graphicsSettings.msaa);
    }
  }
}

void Renderer::updateUBO(Scene& scene) {
  uboData.viewMatrix = scene.camera.getViewMatrix();
  uboData.viewProjectionMatrix = scene.camera.getViewProjectionMatrix();
  uboData.cameraPosition = glm::vec4{scene.camera.getPosition(), 0.0f};

  /*
  // Culling test
glm::vec3 position{15.0f, 5.0f, 15.0f};
glm::vec3 front{0.0f, 0.0f, 1.0f};
glm::vec3 up{0.0f, 1.0f, 0.0f};
glm::mat4 viewMatrix{glm::lookAt(position, position + front, up)};
glm::mat4 projectionMatrix{glm::perspective(
    glm::radians(defaultCameraZoom),
    (float)ConfigWindow::globalGraphicsSettings.screenWidth /
        (float)ConfigWindow::globalGraphicsSettings.screenHeight,
    defaultCameraNearPlane, defaultCameraFarPlane)};

uboData.viewMatrix = viewMatrix;
uboData.viewProjectionMatrix = projectionMatrix * viewMatrix;
uboData.cameraPosition = glm::vec4{position, 0.0f};
//End Culling Test
*/

  void* uboPointer{
      glMapNamedBufferRange(UBO, 0, sizeof(PackedUBOData),
                            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT)};

  std::memcpy(uboPointer, &uboData, sizeof(PackedUBOData));

  glUnmapNamedBuffer(UBO);
}

void Renderer::setupUBO() {
  glCreateBuffers(1, &UBO);

  glNamedBufferStorage(UBO, sizeof(PackedUBOData), nullptr,
                       GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT);

  glBindBufferBase(GL_UNIFORM_BUFFER, 0, UBO);
}

void Renderer::loadShaders() {
  using enum ShaderType;

  if (graphicsSettings.msaa) {
    shaders.emplace(FLOWERS, Shader{"./shaders/flowers.vert",
                                    "./shaders/flowers_msaa.frag"});
    shaders.emplace(
        TREES, Shader{"./shaders/trees.vert", "./shaders/trees_msaa.frag"});
  } else {
    shaders.emplace(FLOWERS,
                    Shader{"./shaders/flowers.vert", "./shaders/flowers.frag"});
    shaders.emplace(TREES,
                    Shader{"./shaders/trees.vert", "./shaders/trees.frag"});
  }

  shaders.emplace(POINT_PBR, Shader{"./shaders/point_PBR.vert",
                                    "./shaders/point_PBR.frag"});
  shaders.emplace(DIRECTIONAL_PBR, Shader{"./shaders/directional_PBR.vert",
                                          "./shaders/directional_PBR.frag"});
  shaders.emplace(REFLECTIVE, Shader{"./shaders/reflective.vert",
                                     "./shaders/reflective.frag"});
  shaders.emplace(LIGHT_SOURCE, Shader{"./shaders/light_source.vert",
                                       "./shaders/light_source.frag"});
  shaders.emplace(SKYBOX,
                  Shader{"./shaders/skybox.vert", "./shaders/skybox.frag"});
}

void Renderer::setupShader(Shader& shader) {
  shader.setUniformBlock("matrices", 0);

  shader.setInt("albedoMap", 0);
  shader.setInt("occlusionRoughMetalMap", 1);
  shader.setInt("normalMap", 2);
  shader.setInt("cubeMap", 3);

  shader.setVec3("ambientLight", ambientLight);
  shader.setVec3("lightColor", lightColor);

  shader.setFloat("windStrength", 0.07f);
  shader.setVec3("windDirection", windDir);

  shader.setFloat("gamma", graphicsSettings.gamma);
}

void Renderer::updateShaderUniforms(Scene& scene) {
  DirectionalLight& globalLight{scene.getGlobalLightSource()};
  bool dirUpdated{globalLight.directionHasUpdated()};

  std::vector<PointLight>& pointLights{scene.getPointLights()};
  glm::vec3 pointPos{pointLights[0].getPosition()};
  const bool posUpdated{pointLights[0].positionHasUpdated()};

  // Set uniforms
  for (auto& [shaderType, shader] : shaders) {
    switch (shaderType) {
      using enum ShaderType;
      case DIRECTIONAL_PBR:
        if (dirUpdated) shader.setVec3("lightDir", globalLight.getDirection());
        break;

      case TREES:
      case FLOWERS:
        shader.setFloat("time", (float)glfwGetTime() * 0.5f);
        if (dirUpdated) shader.setVec3("lightDir", globalLight.getDirection());
        break;

      case POINT_PBR:
        if (posUpdated) shader.setVec3("lightPosition", pointPos);
        break;

      case ShaderType::SKYBOX:
        shader.setMat4("viewProjectionNoTranslation",
                       scene.camera.getProjectionMatrix() *
                           scene.camera.getViewMatrixNoTranslation());
        break;
    }
  }
}

void Renderer::cullRenderables(Scene& scene) {
  const std::array<glm::vec4, 6>& frustumPlanes{
      scene.camera.getFrustumPlanes()};

  for (auto& [_, objectList] : scene.getSceneObjects())
    for (auto& object : objectList) {
      object.isVisible = object.renderable.get().performCulling(
          frustumPlanes, object.transform.getModelMatrix(),
          scene.camera.getPosition());
    }
}
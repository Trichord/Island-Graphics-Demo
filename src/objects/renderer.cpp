#include "renderer.h"

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <iostream>
#include <unordered_map>

#include "camera.h"
#include "config_window.h"
#include "directional_light.h"
#include "renderable.h"
#include "scene.h"
#include "shader.h"
#include "shader_types.h"
#include "transform.h"

Renderer::Renderer(int screenWidth, int screenHeight) {
  // Load shaders
  std::cout << "Loading Shaders...";
  loadShaders();
  std::cout << " Done." << std::endl;

  // Setup shaders
  setupGrassShader(shaders.at(ShaderType::GRASS));
  setupFlowerShader(shaders.at(ShaderType::FLOWERS));
  setupPBRShader(shaders.at(ShaderType::POINT_PBR));
  setupPBRShader(shaders.at(ShaderType::DIRECTIONAL_PBR));
  setupReflectiveShader(shaders.at(ShaderType::REFLECTIVE));
  setupLightSourceShader(shaders.at(ShaderType::LIGHT_SOURCE));
  setupSkyboxShader(shaders.at(ShaderType::SKYBOX));

  // Uniform Buffer Object
  setupUBO();
}

Renderer::Renderer(Renderer&& other) noexcept {
  shaders = std::move(other.shaders);
  uboData = std::move(other.uboData);
  UBO = other.UBO;
  other.UBO = 0;
}

Renderer& Renderer::operator=(Renderer&& other) noexcept {
  if (this != &other) {
    glDeleteBuffers(1, &UBO);
    UBO = other.UBO;
    other.UBO = 0;

    shaders.clear();
    shaders = std::move(other.shaders);
    uboData = std::move(other.uboData);
  }
  return *this;
}

void Renderer::renderScene(Scene& scene) {
  updateShaderUniforms(scene);

  // For each shader type in the scene
  for (auto& [shaderType, objectList] : scene.getSceneObjects()) {
    Shader& shader{shaders.at(shaderType)};
    shader.use();

    // For each object of that shader type
    for (auto& object : objectList) {
      glm::mat4 modelMatrix{object.transform.getModelMatrix()};

      switch (shaderType) {
        // Directional light source
        case ShaderType::DIRECTIONAL_PBR:
          shader.setMat4("modelMatrix", modelMatrix);
          shader.setMat3("normalMV",
                         glm::transpose(glm::inverse(glm::mat3{
                             scene.camera.getViewMatrix() * modelMatrix})));
          break;

        // Point light source
        case ShaderType::POINT_PBR:
          shader.setMat4("modelMatrix", modelMatrix);
          shader.setMat3("normalMV",
                         glm::transpose(glm::inverse(glm::mat3{
                             scene.camera.getViewMatrix() * modelMatrix})));
          break;

        // Reflective shader
        case ShaderType::REFLECTIVE:
          shader.setMat4("modelMatrix", modelMatrix);
          shader.setMat3("normalMatrix", object.transform.getNormalMatrix());
          break;

          // Skyboxes
        case ShaderType::SKYBOX:
          shader.setMat4("projectionViewNoTranslation",
                         scene.camera.getProjectionMatrix() *
                             scene.camera.getViewNoTranslationMatrix());
          break;

        case ShaderType::LIGHT_SOURCE:
          shader.setMat4("modelMatrix", modelMatrix);
          break;
      }

      // Draw it
      object.renderable.get().render();
    }
  }
}

void Renderer::updateUBO(Scene& scene) {
  uboData.viewMatrix = scene.camera.getViewMatrix();
  uboData.projectionViewMatrix =
      scene.camera.getProjectionMatrix() * scene.camera.getViewMatrix();
  uboData.cameraPosition = glm::vec4{scene.camera.getPosition(), 0.0f};
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PackedUBOData), &uboData);
}

void Renderer::setupUBO() {
  glGenBuffers(1, &UBO);
  glBindBuffer(GL_UNIFORM_BUFFER, UBO);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(PackedUBOData), NULL, GL_STATIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, UBO);
}

void Renderer::loadShaders() {
  using enum ShaderType;
  shaders.emplace(GRASS,
                  Shader("./shaders/grass.vert", "./shaders/grass.frag"));
  shaders.emplace(FLOWERS,
                  Shader("./shaders/flowers.vert", "./shaders/flowers.frag"));
  shaders.emplace(POINT_PBR, Shader("./shaders/point_PBR.vert",
                                    "./shaders/point_PBR.frag"));
  shaders.emplace(DIRECTIONAL_PBR, Shader("./shaders/directional_PBR.vert",
                                          "./shaders/directional_PBR.frag"));
  shaders.emplace(REFLECTIVE, Shader("./shaders/reflective.vert",
                                     "./shaders/reflective.frag"));
  shaders.emplace(LIGHT_SOURCE, Shader("./shaders/light_source.vert",
                                       "./shaders/light_source.frag"));
  shaders.emplace(SKYBOX,
                  Shader("./shaders/skybox.vert", "./shaders/skybox.frag"));
}

void Renderer::setupGrassShader(Shader& shader) {
  glm::vec3 windDirection{0.88f, -0.1f, 0.4344f};

  shader.setUniformBlock("matrices", 0);
  shader.setInt("colorTexture", 0);
  shader.setFloat("gamma", ConfigWindow::globalGraphicsSettings.gamma);
  shader.setFloat("windStrength", 0.07f);
  shader.setVec3("windDirection", glm::normalize(windDirection));
}

void Renderer::setupFlowerShader(Shader& shader) {
  glm::vec3 windDirection{0.88f, -0.1f, 0.4344f};

  shader.setUniformBlock("matrices", 0);
  shader.setInt("colorTexture", 0);
  shader.setFloat("gamma", ConfigWindow::globalGraphicsSettings.gamma);
  shader.setFloat("windStrength", 0.07f);
  shader.setVec3("windDirection", glm::normalize(windDirection));
}

void Renderer::setupPBRShader(Shader& shader) {
  shader.setUniformBlock("matrices", 0);
  shader.setFloat("gamma", ConfigWindow::globalGraphicsSettings.gamma);
  shader.setInt("albedoMap", 0);
  shader.setInt("occlusionRoughMetalMap", 1);
  shader.setInt("normalMap", 2);
}

void Renderer::setupReflectiveShader(Shader& shader) {
  shader.setUniformBlock("matrices", 0);
  shader.setFloat("gamma", ConfigWindow::globalGraphicsSettings.gamma);
  shader.setInt("cubemap", 0);
}

void Renderer::setupLightSourceShader(Shader& shader) {
  shader.setUniformBlock("matrices", 0);
}
void Renderer::setupSkyboxShader(Shader& shader) {
  shader.setInt("skybox", 0);
  shader.setFloat("gamma", ConfigWindow::globalGraphicsSettings.gamma);
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

      case POINT_PBR:
        if (posUpdated) shader.setVec3("lightPosition", pointPos);
        break;

      case GRASS:
        shader.setFloat("time", (float)glfwGetTime() * 0.5f);
        if (dirUpdated) shader.setVec3("lightDir", globalLight.getDirection());
        break;

      case FLOWERS:
        shader.setFloat("time", (float)glfwGetTime() * 0.5f);
        if (dirUpdated) shader.setVec3("lightDir", globalLight.getDirection());
        break;
    }
  }
}
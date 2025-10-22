#include <glad/glad.h>
#include "../headers/renderer.h"
#include "../headers/renderable.h"
#include "../headers/transform.h"
#include "../headers/scene.h"
#include "../headers/shader_types.h"
#include "../headers/shader.h"
#include "../headers/camera.h"
#include "../headers/config_window.h"
#include <unordered_map>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#define CAMERA_ZOOM 45.0f
#define NEAR_PLANE 0.1f
#define FAR_PLANE 600.0f

Renderer::Renderer(int screenWidth, int screenHeight)
    : projectionMatrix(glm::perspective(
          glm::radians(CAMERA_ZOOM), (float)screenWidth / (float)screenHeight,
          NEAR_PLANE, FAR_PLANE)) {
  // Load shaders
  std::cout << "Loading Shaders...";
  loadShaders();
  std::cout << " Done." << std::endl;

  // Setup shaders
  setupGrassShader(shaders.at(ShaderType::GRASS));
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
  projectionMatrix = std::move(other.projectionMatrix);
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
    projectionMatrix = std::move(other.projectionMatrix);
  }
  return *this;
}

void Renderer::renderScene(Scene& scene) {
  updateShaderUniforms(scene);

  // For each shader type in the scene
  for (auto& [shaderType, objectList] : scene.getSceneObjects()) {
    Shader& shader = shaders.at(shaderType);
    shader.use();

    // For each object of that shader type
    for (auto& object : objectList) {
      glm::mat4 modelMatrix = object.transform.getModelMatrix();

      switch (shaderType) {
        // Directional light source
        case ShaderType::DIRECTIONAL_PBR:
          shader.setMat4("modelMatrix", modelMatrix);
          shader.setMat3("normalMV",
                         glm::transpose(glm::inverse(glm::mat3(
                             scene.camera.getViewMatrix() * modelMatrix))));
          break;

        // Point light source
        case ShaderType::POINT_PBR:
          shader.setMat4("modelMatrix", modelMatrix);
          shader.setMat3("normalMV",
                         glm::transpose(glm::inverse(glm::mat3(
                             scene.camera.getViewMatrix() * modelMatrix))));
          break;

        // Reflective shader
        case ShaderType::REFLECTIVE:
          shader.setMat4("modelMatrix", modelMatrix);
          shader.setMat3("normalMatrix", object.transform.getNormalMatrix());
          break;

          // Skyboxes
        case ShaderType::SKYBOX:
          shader.setMat4(
              "projectionViewNoTranslation",
              projectionMatrix * scene.camera.getViewNoTranslationMatrix());
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
      projectionMatrix * scene.camera.getViewMatrix();
  uboData.cameraPosition = glm::vec4(scene.camera.getPosition(), 0.0f);
  glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PackedUBOData), &uboData);
}

void Renderer::setupUBO() {
  glGenBuffers(1, &UBO);
  glBindBuffer(GL_UNIFORM_BUFFER, UBO);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(PackedUBOData), NULL, GL_STATIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, UBO);
}

void Renderer::loadShaders() {
  shaders.emplace(ShaderType::GRASS,
                  Shader("./shaders/grass.vert", "./shaders/grass.frag"));
  shaders.emplace(ShaderType::POINT_PBR, Shader("./shaders/point_PBR.vert",
                                                "./shaders/point_PBR.frag"));
  shaders.emplace(ShaderType::DIRECTIONAL_PBR,
                  Shader("./shaders/directional_PBR.vert",
                         "./shaders/directional_PBR.frag"));
  shaders.emplace(ShaderType::REFLECTIVE, Shader("./shaders/reflective.vert",
                                                 "./shaders/reflective.frag"));
  shaders.emplace(
      ShaderType::LIGHT_SOURCE,
      Shader("./shaders/light_source.vert", "./shaders/light_source.frag"));
  shaders.emplace(ShaderType::SKYBOX,
                  Shader("./shaders/skybox.vert", "./shaders/skybox.frag"));
}

void Renderer::setupGrassShader(Shader& shader) {
  shader.setUniformBlock("matrices", 0);
  shader.setInt("grassTexture", 0);
  shader.setFloat("gamma", ConfigWindow::globalGraphicsSettings.gamma);
  shader.setFloat("windStrength", 0.07f);
  shader.setVec3("windDirection",
                 glm::normalize(glm::vec3(0.88f, -0.1f, 0.4344f)));
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
  // Get light info (only setup for 1 light at the moment)
  std::vector<DirectionalLight>& dirLights = scene.getDirectionalLights();
  bool dirUpdated = dirLights[0].directionHasUpdated();

  std::vector<PointLight>& pointLights = scene.getPointLights();
  glm::vec3 pointPos = pointLights[0].getPosition();
  bool posUpdated = pointLights[0].positionHasUpdated();

  // Set uniforms
  for (auto& [shaderType, shader] : shaders) {
    switch (shaderType) {
      case ShaderType::DIRECTIONAL_PBR:
        if (dirUpdated) shader.setVec3("lightDir", dirLights[0].getDirection());
        break;

      case ShaderType::POINT_PBR:
        if (posUpdated) shader.setVec3("lightPosition", pointPos);
        break;

      case ShaderType::GRASS:
        shader.setFloat("time", (float)glfwGetTime() * 0.5f);
        if (dirUpdated) shader.setVec3("lightDir", dirLights[0].getDirection());
        break;
    }
  }
}
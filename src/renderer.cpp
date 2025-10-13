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
  // Get light info (only setup for 1 light at the moment)
  glm::vec3 dirLight = scene.directionalLights[0].getDirection();
  bool dirUpdated = scene.directionalLights[0].directionHasUpdated();

  glm::vec3 pointLight = scene.pointLights[0].getPosition();
  bool posUpdated = scene.pointLights[0].positionHasUpdated();

  // For each shader type in the scene
  for (auto& [shaderType, objectList] : scene.getSceneObjects()) {
    Shader& shader = shaders.at(shaderType);
    shader.use();

    // Set uniforms
    if (shaderType == ShaderType::DIRECTIONAL_PBR && dirUpdated)
      shader.setVec3("lightDir", dirLight);

    else if (shaderType == ShaderType::POINT_PBR && posUpdated)
      shader.setVec3("lightPosition", pointLight);

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
      }

      // Draw it
      object.renderable.get().render(shader);
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

void Renderer::setupPBRShader(Shader& shader) {
  shader.use();
  shader.setUniformBlock("matrices", 0);
  shader.setFloat("gamma", ConfigWindow::globalGraphicsSettings.gamma);
  shader.setInt("albedoMap", 0);
  shader.setInt("occlusionRoughMetalMap", 1);
  shader.setInt("normalMap", 2);
}

void Renderer::setupReflectiveShader(Shader& shader) {
  shader.use();
  shader.setUniformBlock("matrices", 0);
  shader.setFloat("gamma", ConfigWindow::globalGraphicsSettings.gamma);
  shader.setInt("cubemap", 0);
}

void Renderer::setupLightSourceShader(Shader& shader) {
  shader.use();
  shader.setUniformBlock("matrices", 0);
}
void Renderer::setupSkyboxShader(Shader& shader) {
  shader.use();
  shader.setInt("skybox", 0);
}
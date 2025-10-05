#include <glad/glad.h>
#include "../headers/renderer.h"
#include "../headers/renderable.h"
#include "../headers/scene.h"
#include "../headers/shader_types.h"
#include "../headers/shader.h"
#include "../headers/camera.h"
#include <unordered_map>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define CAMERA_ZOOM 45.0f
#define NEAR_PLANE 0.1f
#define FAR_PLANE 600.0f

Renderer::Renderer(const ShaderManager& manager, Camera& camera,
                   int screenWidth, int screenHeight)
    : shaderManager(manager) {
  projectionMatrix = glm::perspective(glm::radians(CAMERA_ZOOM),
                                      (float)screenWidth / (float)screenHeight,
                                      NEAR_PLANE, FAR_PLANE);
  setupUBO(camera);
}

void Renderer::renderScene(Scene& scene) {
  for (auto& [shaderType, objectList] : scene.getSceneObjects()) {
    Shader& shader = shaderManager.getShaders().at(shaderType);
    shader.use();

   for (auto& object : objectList) {
      shader.setMat4("modelMatrix", object.transform.
    
   }

  }
}

void Renderer::updateUBO(Camera& camera) {
  uboData.viewMatrix = camera.getViewMatrix();
  uboData.projectionViewMatrix = projectionMatrix * camera.getViewMatrix();
  glBufferSubData(GL_UNIFORM_BUFFER, 0, 2 * sizeof(PackedUBOData), &uboData);
}

void Renderer::setupUBO(Camera& camera) {
  glGenBuffers(1, &UBO);
  glBindBuffer(GL_UNIFORM_BUFFER, UBO);
  glBufferData(GL_UNIFORM_BUFFER, sizeof(PackedUBOData), NULL, GL_STATIC_DRAW);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, UBO);
  updateUBO(camera);
}
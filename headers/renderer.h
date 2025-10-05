#pragma once

#include <map>
#include <unordered_map>
#include <glm/glm.hpp>

#include "scene.h"
#include "shader.h"
#include "shader_types.h"
#include "renderable.h"
#include "camera.h"

class Renderer {
 public:
  Renderer() = delete;
  Renderer(Camera& camera, int screenWidth, int screenHeight);
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  Renderer(Renderer&& other) noexcept;
  Renderer& operator=(Renderer&& other) noexcept;

  void renderScene(Scene& scene, Camera& camera);
  void updateUBO(Camera& camera);

 private:
  // Packed UBO data holds everything we want to share with all shaders
  struct PackedUBOData {
    glm::mat4 viewMatrix;
    glm::mat4 projectionViewMatrix;
  };

  PackedUBOData uboData;
  std::unordered_map<ShaderType, Shader> shaders;
  glm::mat4 projectionMatrix;
  unsigned int UBO;

  void setupUBO(Camera& camera);
  void loadShaders();
  void setupPBRShader(Shader& shader);
  void setupReflectiveShader(Shader& shader);
  void setupLightSourceShader(Shader& shader);
  void setupSkyboxShader(Shader& shader);
};

#pragma once

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <unordered_map>

#include "scene.h"
#include "shader.h"
#include "shader_types.h"

class Renderer {
 public:
  Renderer() = delete;
  Renderer(int screenWidth, int screenHeight);
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  Renderer(Renderer&& other) noexcept;
  Renderer& operator=(Renderer&& other) noexcept;

  void renderScene(Scene& scene);
  void updateUBO(Scene& scene);

 private:
  // Packed UBO data holds everything we want to share with all shaders
  struct PackedUBOData {
    glm::mat4 viewMatrix;
    glm::mat4 projectionViewMatrix;
    glm::vec4 cameraPosition;
  };

  GLuint UBO{0};
  PackedUBOData uboData;
  std::unordered_map<ShaderType, Shader> shaders;

  void setupUBO();
  void loadShaders();
  void setupGrassShader(Shader& shader);
  void setupFlowerShader(Shader& shader);
  void setupPBRShader(Shader& shader);
  void setupReflectiveShader(Shader& shader);
  void setupLightSourceShader(Shader& shader);
  void setupSkyboxShader(Shader& shader);
  void updateShaderUniforms(Scene& scene);
};

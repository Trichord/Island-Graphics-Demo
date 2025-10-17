#pragma once

#include <string>
#include <vector>

#include "../headers/cube_mapped_model.h"
#include "../headers/shader.h"
#include "glad/glad.h"

class Skybox : public CubeMappedModel {
 public:
  Skybox() = delete;
  explicit Skybox(const std::vector<std::string>& skyboxPaths);
  Skybox(const Skybox&) = delete;
  Skybox& operator=(const Skybox&) = delete;
  Skybox(Skybox&& other) noexcept;
  Skybox& operator=(Skybox&& other) noexcept;
  ~Skybox();

  void render(Shader& shader) override;

 protected:
  GLuint modelVAO = 0, modelVBO = 0, modelEBO = 0;

  inline static const float vertices[] = {
      -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f, 1.0f,
      -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, 1.0f, 1.0f,
      -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f, 1.0f};

  inline static const unsigned int indices[] = {
      0, 1, 2, 2, 3, 0, 5, 4, 7, 7, 6, 5, 4, 0, 3, 3, 7, 4,
      1, 5, 6, 6, 2, 1, 4, 5, 1, 1, 0, 4, 3, 2, 6, 6, 7, 3};

  void setupBuffers();
  void deleteBuffers();
};
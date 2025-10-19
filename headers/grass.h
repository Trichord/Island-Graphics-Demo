#pragma once

#include "renderable.h"
#include "glad/glad.h"
#include "shader.h"
#include <filesystem>
#include <vector>
#include <glm/glm.hpp>

class Grass : public Renderable {
 public:
  Grass(const std::filesystem::path &positionsPath,
        const std::filesystem::path &texturePath,
        unsigned int numPositions = 0);
  Grass(const Grass &) = delete;
  Grass &operator=(const Grass &) = delete;
  Grass(Grass &&other) noexcept;
  Grass &operator=(Grass &&other) noexcept;
  ~Grass();

  void render(Shader &shader) override;

 private:
  GLuint textureID = 0;
  GLuint VAO = 0, VBO = 0, EBO = 0, transformsVBO = 0, normalsVBO = 0;
  std::vector<glm::mat4> transforms;
  std::vector<glm::mat3> normalMatrices;
  std::vector<glm::vec3> grassPositions;

  // Simple centered quad
  inline static const float vertices[] = {
      // vertex           //normal          //uv
      -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,   // Top Left
      0.5f,  1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,   // Top Right
      -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,   // Bottom Left
      0.5f,  0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};  // Bottom Right

  inline static const unsigned int indices[] = {0, 2, 3, 0, 3, 1};

  void loadTexture(const std::filesystem::path &texturePath);
  void setupBuffers();
  void generateTransforms();
  void deleteBuffers();
  void loadGrassPositions(const std::filesystem::path &positionsPath,
                          unsigned int numPositions);
};
#pragma once

#include "renderable.h"
#include "glad/glad.h"
#include "shader.h"
#include <filesystem>
#include <vector>
#include <glm/glm.hpp>

class Grass : public Renderable {
 public:
  enum TuftPattern { STAR_PATTERN, RADIAL_PATTERN };

  Grass(const std::filesystem::path &positionsPath,
        const std::filesystem::path &texturePath, TuftPattern tuftPattern,
        float minSize, float maxSize, unsigned int tuftDensity = 3,
        unsigned int numPositions = 0);
  Grass(const Grass &) = delete;
  Grass &operator=(const Grass &) = delete;
  Grass(Grass &&other) noexcept;
  Grass &operator=(Grass &&other) noexcept;
  ~Grass();

  void render() override;

 private:
  GLuint textureID = 0;
  GLuint VAO = 0, VBO = 0, EBO = 0, transformsVBO = 0, positionsVBO = 0;
  std::vector<glm::mat4> transforms;
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
  void setupBuffers(unsigned int tuftDensity);
  void generateRadialPatternTransforms(float minSize, float maxSize,
                                       unsigned int tuftDensity);
  void generateStarPatternTransforms(float minSize, float maxSize,
                                     unsigned int tuftDensity);
  void deleteBuffers();
  void loadGrassPositions(const std::filesystem::path &positionsPath,
                          unsigned int numPositions);
};
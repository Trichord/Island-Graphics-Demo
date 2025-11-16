#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <vector>

#include "glad/glad.h"
#include "renderable.h"
#include "texture_manager.h"

class Quad : public Renderable {
 public:
  enum QuadPattern { STAR_PATTERN, RADIAL_PATTERN };

  Quad(TextureManager &textureManager, const std::filesystem::path &texturePath,
       const std::filesystem::path &positionsPath, QuadPattern pattern,
       float minSize = 1.0f, float maxSize = 1.0f, unsigned int density = 1,
       unsigned int numPositions = 0);
  Quad(const Quad &) = delete;
  Quad &operator=(const Quad &) = delete;
  Quad(Quad &&other) noexcept;
  Quad &operator=(Quad &&other) = delete;
  ~Quad();

  void render() override;

 private:
  GLuint VAO{0};
  GLuint VBO{0};
  GLuint EBO{0};
  GLuint transformsVBO{0};
  GLuint positionsVBO{0};
  std::vector<glm::mat4> transforms;
  std::vector<glm::vec3> positions;
  ModelHandle handle{0};
  TextureManager &texManager;
  const std::vector<TextureSet> &textureSet;

  // Simple centered quad
  inline static const float vertices[]{
      // vertex           //normal          //uv
      -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,   // Top Left
      0.5f,  1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,   // Top Right
      -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,   // Bottom Left
      0.5f,  0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};  // Bottom Right

  inline static const unsigned int indices[]{0, 2, 3, 0, 3, 1};

  void setupBuffers(unsigned int density);
  void generateRadialPatternTransforms(float minSize, float maxSize,
                                       unsigned int density);
  void generateStarPatternTransforms(float minSize, float maxSize,
                                     unsigned int density);
  void deleteBuffers();
  void loadPositions(const std::filesystem::path &positionsPath,
                     unsigned int numPositions);
};
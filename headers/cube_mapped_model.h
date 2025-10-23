#pragma once

#include <glad/glad.h>

#include <filesystem>
#include <string>
#include <vector>

#include "renderable.h"
#include "shader.h"

// Cube mapped models handle their VAO's and VBO's differently, so default
// behavior is no copy. texturePaths should be filled in this order: right,
// left, top, bottom, front, back

class CubeMappedModel : public Renderable {
 public:
  CubeMappedModel() = default;
  CubeMappedModel(const CubeMappedModel &) = delete;
  CubeMappedModel &operator=(const CubeMappedModel &) = delete;
  CubeMappedModel(CubeMappedModel &&other) noexcept;
  CubeMappedModel &operator=(CubeMappedModel &&other) noexcept;
  virtual ~CubeMappedModel();

  void render() override = 0;

 protected:
  GLuint cubemapTextureID{0};
  std::vector<std::filesystem::path> texturePaths;
  void loadTextures();
};
#pragma once

#include <glad/glad.h>

#include <filesystem>
#include <vector>

#include "renderable.h"
#include "texture_manager.h"

/* Cube mapped models handle their VAO 's and VBO' s differently, so default
 * behavior is no copy. texturePaths should be filled in this order: right,
 * left, top, bottom, front, back */

class CubeMappedModel : public Renderable {
 public:
  explicit CubeMappedModel(TextureManager &textureManager);
  CubeMappedModel(TextureManager &textureManager, const CubeMappedModel &other);
  CubeMappedModel(const CubeMappedModel &) = delete;
  CubeMappedModel &operator=(const CubeMappedModel &) = delete;
  CubeMappedModel(CubeMappedModel &&other) noexcept;
  CubeMappedModel &operator=(CubeMappedModel &&other) = delete;
  virtual ~CubeMappedModel();

  void render() override = 0;
  ModelHandle getModelHandle();

 protected:
  ModelHandle handle{0};
  TextureManager &texManager;
  const std::vector<TextureSet> &textureSet;
};
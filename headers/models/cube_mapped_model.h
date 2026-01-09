#pragma once

#include <filesystem>
#include <vector>

#include "model.h"
#include "texture_manager.h"

/* Cube mapped texture paths should be filled in this order: right,
 * left, top, bottom, front, back */

class CubeMappedModel : public Model {
 public:
  CubeMappedModel(TextureManager& textureManager,
                  const std::filesystem::path& modelPath,
                  const std::vector<std::filesystem::path>& imagePaths);
  CubeMappedModel(TextureManager& textureManager,
                  const CubeMappedModel& borrowTarget,
                  const std::filesystem::path& modelPath,
                  const std::vector<std::filesystem::path>& imagePaths);
};
#pragma once

#include <filesystem>
#include <vector>

#include "model.h"
#include "texture_manager.h"

/* Cube mapped texture paths should be filled in this order: right,
 * left, top, bottom, front, back */

class CubeMappedModel : public Model
{
  public:
    CubeMappedModel(TextureManager &textureManager, const std::filesystem::path &modelPath,
                    const std::vector<std::filesystem::path> &imagePaths, bool skybox);
    CubeMappedModel(TextureManager &textureManager, const ModelHandle borrowHandle,
                    const std::filesystem::path &modelPath);
};
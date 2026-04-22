#pragma once

#include <filesystem>
#include <vector>

#include "base_model.h"
#include "texture_manager.h"

/* Cube mapped texture paths should be filled in this order: right,
 * left, top, bottom, front, back */

class CubeMappedModel : public BaseModel
{
  public:
    CubeMappedModel(TextureManager &textureManager, const std::filesystem::path &modelPath,
                    const std::vector<std::filesystem::path> &imagePaths, bool mipmaps);
    CubeMappedModel(TextureManager &textureManager, const ModelHandle borrowHandle,
                    const std::filesystem::path &modelPath);
};
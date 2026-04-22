#include "cube_mapped_model.h"

#include <filesystem>
#include <vector>

#include "base_model.h"
#include "texture_manager.h"

CubeMappedModel::CubeMappedModel(TextureManager &textureManager, const std::filesystem::path &modelPath,
                                 const std::vector<std::filesystem::path> &imagePaths, bool mipmaps)
    : BaseModel{textureManager, modelPath}
{
    textureManager.loadCubemapTextures(handle_, imagePaths, "./", mipmaps);
}

CubeMappedModel::CubeMappedModel(TextureManager &textureManager, const ModelHandle borrowHandle,
                                 const std::filesystem::path &modelPath)
    : BaseModel{textureManager, borrowHandle, modelPath}
{
}
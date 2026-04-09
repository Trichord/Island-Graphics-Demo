#include "cube_mapped_model.h"

#include <filesystem>
#include <vector>

#include "model.h"
#include "texture_manager.h"

CubeMappedModel::CubeMappedModel(TextureManager &textureManager, const std::filesystem::path &modelPath,
                                 const std::vector<std::filesystem::path> &imagePaths, bool skybox)
    : Model{textureManager, modelPath}
{
    textureManager.loadCubemap(handle_, imagePaths, "./", skybox);
}

CubeMappedModel::CubeMappedModel(TextureManager &textureManager, const ModelHandle borrowHandle,
                                 const std::filesystem::path &modelPath)
    : Model{textureManager, borrowHandle, modelPath}
{
}
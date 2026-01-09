#include "cube_mapped_model.h"

#include <filesystem>
#include <vector>

#include "texture_manager.h"

CubeMappedModel::CubeMappedModel(
    TextureManager& textureManager, const std::filesystem::path& modelPath,
    const std::vector<std::filesystem::path>& imagePaths)
    : Model{textureManager, modelPath} {
  textureManager.queueCubemapTextureSet(handle, imagePaths, "./");

  if (!hasMaterials()) assignMaterialIndex(0);
}

CubeMappedModel::CubeMappedModel(
    TextureManager& textureManager, const CubeMappedModel& borrowTarget,
    const std::filesystem::path& modelPath,
    const std::vector<std::filesystem::path>& imagePaths)
    : Model{textureManager, borrowTarget, modelPath} {
  if (!hasMaterials()) assignMaterialIndex(0);
}
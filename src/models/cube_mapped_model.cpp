#include "cube_mapped_model.h"

#include <glad/glad.h>

#include <filesystem>
#include <vector>

#include "image_loader.h"
#include "texture_utilities.h"
#include "texture_manager.h"

CubeMappedModel::CubeMappedModel(
    TextureManager& textureManager,
                       const std::filesystem::path& modelPath,
                       const std::vector<std::filesystem::path>& imagePaths)
    : Model{textureManager, modelPath} {
  textureManager.queueCubemapTextureSet(handle, imagePaths, "./");
}

CubeMappedModel::CubeMappedModel(
    TextureManager& textureManager, const CubeMappedModel& borrowTarget,
                       const std::filesystem::path& modelPath,
                       const std::vector<std::filesystem::path>& imagePaths)
    : Model{textureManager, borrowTarget, modelPath} {}
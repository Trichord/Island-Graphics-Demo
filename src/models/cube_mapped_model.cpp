#include "cube_mapped_model.h"

#include <glad/glad.h>

#include <filesystem>
#include <vector>

#include "image_loader.h"
#include "texture_utilities.h"
#include "texture_manager.h"

CubeMappedModel::CubeMappedModel(
    TextureManager& textureManager,
    const std::vector<std::filesystem::path>& imagePaths)
    : texManager{textureManager},
      handle{textureManager.assignModelHandle()},
      textureSet{textureManager.acquireTextureSet(handle)} {}

CubeMappedModel::CubeMappedModel(CubeMappedModel&& other) noexcept
    : handle{other.handle},
      texManager{other.texManager},
      textureSet{other.textureSet} {
  other.handle = 0;
}

CubeMappedModel::~CubeMappedModel() {
  if (handle) texManager.releaseModel(handle);
}

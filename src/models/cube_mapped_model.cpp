#include "cube_mapped_model.h"

#include <glad/glad.h>

#include <filesystem>
#include <vector>

#include "image_loader.h"
#include "texture_utilities.h"
#include "texture_manager.h"

CubeMappedModel::CubeMappedModel(TextureManager& textureManager)
    : texManager{textureManager},
      handle{textureManager.assignModelHandle()},
      textureSet{textureManager.acquireUniqueTextureSet(handle)} {}

CubeMappedModel::CubeMappedModel(TextureManager& textureManager,
                                 const CubeMappedModel& borrowTarget)
    : texManager{textureManager},
      handle{borrowTarget.handle},
      textureSet{textureManager.borrowTextureSet(borrowTarget.handle)} {}

CubeMappedModel::CubeMappedModel(CubeMappedModel&& other) noexcept
    : handle{other.handle},
      texManager{other.texManager},
      textureSet{other.textureSet} {
  other.handle = 0;
}

CubeMappedModel::~CubeMappedModel() {
  if (handle) texManager.releaseModel(handle);
}

ModelHandle CubeMappedModel::getModelHandle() { return handle; }
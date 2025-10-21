#include "../headers/cube_mapped_model.h"

#include <glad/glad.h>

#include <filesystem>
#include <glm/glm.hpp>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../headers/image_loader.h"
#include "../headers/texture_utilities.h"

CubeMappedModel::CubeMappedModel(CubeMappedModel&& other) noexcept
    : cubemapTextureID(other.cubemapTextureID) {
  other.cubemapTextureID = 0;
}

CubeMappedModel& CubeMappedModel::operator=(CubeMappedModel&& other) noexcept {
  if (this != &other) {
    glDeleteTextures(1, &cubemapTextureID);
    cubemapTextureID = other.cubemapTextureID;
    other.cubemapTextureID = 0;
  }

  return *this;
}

CubeMappedModel::~CubeMappedModel() { glDeleteTextures(1, &cubemapTextureID); }

void CubeMappedModel::loadTextures() {
  ImageLoader imageLoader(texturePaths);
  std::vector<ImageData> loadedImages = imageLoader.getImageData();
  cubemapTextureID =
      texture_utilities::generateCubemapTextureID(loadedImages, true, false);
}

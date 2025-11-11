#include "TextureManager.h"
#include "glad/glad.h"

#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <random>
#include <limits>

const ModelHandle TextureManager::createModelTextureSet(
    const fastgltf::TextureInfo& textureInfo, bool sRGB = true) {}

const ModelHandle TextureManager::createModelTextureSet(
    const std::filesystem::path& texturePath, bool sRGB = true) {}

const ModelHandle TextureManager::createModelTextureSet(
    const std::vector<std::filesystem::path>& imagePaths, bool sRGB = true) {}

const std::unordered_set<TextureHandle>& TextureManager::acquireTextureSet(
    ModelHandle modelHandle) {}

void TextureManager::releaseModel(ModelHandle modelHandle) {
  if (modelTextures.find(modelHandle) == modelTextures.end()) return;

  std::unordered_set<TextureHandle>& modelTextureHandles{
      modelTextures[modelHandle]};

  for (auto& textureHandle : modelTextureHandles) {
    if (textureCache.find(textureHandle) == textureCache.end()) continue;

    TextureEntry& entry{textureCache[textureHandle]};

    --entry.refCount;

    if (entry.refCount <= 0) {
      deleteTextureIDs(textureHandle);
      textureCache.erase(textureHandle);
    }
  }

  if (!modelTextures[modelHandle].size()) modelTextures.erase(modelHandle);
}

uint64_t TextureManager::createUniqueHandle() const {
  std::random_device rd;
  std::mt19937 mt(rd());
  uint64_t maxValue{std::numeric_limits<uint64_t>::max()};

  std::uniform_int_distribution<uint64_t> rndHandleRange{1, maxValue};

  uint64_t handle{rndHandleRange(mt)};

  while (textureCache.contains(handle) || modelTextures.contains(handle))
    handle = rndHandleRange(mt);

  return handle;
}

void TextureManager::deleteTextureIDs(TextureHandle textureHandle) {
  if (textureCache.find(textureHandle) != textureCache.end()) {
    const TextureSet& textureIDs{textureCache[textureHandle].textureSet};

    if (textureIDs.albedo) glDeleteTextures(1, &textureIDs.albedo);

    if (textureIDs.occlusionRoughnessMetallic)
      glDeleteTextures(1, &textureIDs.occlusionRoughnessMetallic);

    if (textureIDs.normal) glDeleteTextures(1, &textureIDs.normal);

    if (textureIDs.cubemap) glDeleteTextures(1, &textureIDs.cubemap);
  }
}
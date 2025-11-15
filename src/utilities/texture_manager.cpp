#include "texture_manager.h"
#include "glad/glad.h"
#include "thread_pool.h"
#include "model.h"

#include <unordered_map>
#include <vector>
#include <cstdint>
#include <utility>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <random>
#include <limits>
#include <iostream>

const ModelHandle TextureManager::queueModelTextureSet(const ModelHandle handle,
                                                       fastgltf::Scene&& scene,
                                                       bool sRGB) {
  textureCreationQueue.emplace(std::move(scene), handle, sRGB);
}

const ModelHandle TextureManager::queueBaseImageTextureSet(
    const ModelHandle handle, std::filesystem::path texturePath, bool sRGB) {
  textureCreationQueue.emplace(std::move(texturePath), handle, sRGB);
}

const ModelHandle TextureManager::queueCubemapTextureSet(
    const ModelHandle handle, std::vector<std::filesystem::path> imagePaths,
    bool sRGB) {
  textureCreationQueue.emplace(std::move(imagePaths), handle, sRGB);
}

const std::vector<TextureSet>& TextureManager::acquireTextureSet(
    ModelHandle modelHandle) const {
  auto textureSetIter = textureCache.find(modelHandle);

  if (textureSetIter == textureCache.end()) {
    std::cerr << "model handle not found" << std::endl;
    return textureCache.at(0).textureSets;
  }

  return textureSetIter->second.textureSets;
}

void TextureManager::releaseModel(ModelHandle modelHandle) {
  auto modelIter{textureCache.find(modelHandle)};

  if (modelIter == textureCache.end()) return;

  TextureEntry& entry{modelIter->second};
  entry.refCount--;

  if (entry.refCount == 0) {
    for (auto& textureSet : entry.textureSets) deleteTextureIDs(textureSet);

    textureCache.erase(modelIter);
  }
}

uint64_t TextureManager::createUniqueHandle() const {
  std::random_device rd;
  std::mt19937 mt(rd());
  constexpr uint64_t maxValue{std::numeric_limits<uint64_t>::max()};

  std::uniform_int_distribution<uint64_t> rndHandleRange{1, maxValue};

  uint64_t handle{rndHandleRange(mt)};

  while (textureCache.contains(handle)) handle = rndHandleRange(mt);

  return handle;
}

void TextureManager::deleteTextureIDs(TextureSet textureSet) {
  if (textureSet.albedo) glDeleteTextures(1, &textureSet.albedo);

  if (textureSet.occlusionRoughnessMetallic)
    glDeleteTextures(1, &textureSet.occlusionRoughnessMetallic);

  if (textureSet.normal) glDeleteTextures(1, &textureSet.normal);

  if (textureSet.cubemap) glDeleteTextures(1, &textureSet.cubemap);
}

void TextureManager::processTextureQueue() {
  if (textureCreationQueue.empty()) return;

  ThreadPool threadPool{6u};

  while (!textureCreationQueue.empty()) {
    auto& queueElement = textureCreationQueue.front();

    if (std::holds_alternative<fastgltf::Scene>(
            queueElement.source)) {
    }

    textureCreationQueue.pop();
  }
}
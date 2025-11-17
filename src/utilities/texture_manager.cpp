#include "texture_manager.h"
#include "glad/glad.h"
#include "thread_pool.h"
#include "model.h"
#include "fastgltf_loader.h"
#include "texture_utilities.h"

#include <unordered_map>
#include <stddef.h>
#include <vector>
#include <cstdint>
#include <utility>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <unordered_set>
#include <random>
#include <limits>
#include <iostream>

constexpr unsigned int desiredThreadCount{6u};

void TextureManager::queueModelTextureSet(
    const ModelHandle handle, fastgltf::Asset&& asset,
    const std::filesystem::path& parentPath) {
  textureCreationQueue.emplace(std::move(asset), handle, parentPath);
}

void TextureManager::queueBaseImageTextureSet(
    const ModelHandle handle, std::filesystem::path texturePath) {
  textureCreationQueue.emplace(std::move(texturePath), handle, "./");
}

void TextureManager::queueCubemapTextureSet(
    const ModelHandle handle, std::vector<std::filesystem::path> imagePaths,
    const std::filesystem::path& parentPath, bool aniso) {
  textureCreationQueue.emplace(std::move(imagePaths), handle, parentPath,
                               aniso);
}

const std::vector<TextureSet>& TextureManager::acquireUniqueTextureSet(
    ModelHandle modelHandle) const {
  auto textureSetIter = textureCache.find(modelHandle);

  if (textureSetIter == textureCache.end()) {
    std::cerr << "model handle not found" << std::endl;
    static const std::vector<TextureSet> empty;
    return empty;
  }

  return textureSetIter->second.textureSets;
}

void TextureManager::releaseModel(ModelHandle modelHandle) {
  auto modelIter{textureCache.find(modelHandle)};

  if (modelIter == textureCache.end()) return;

  TextureEntry& entry{modelIter->second};
  entry.refCount--;

  if (entry.refCount == 0) {
    for (auto& gpuHandle : entry.handlesInUse) glDeleteTextures(1, &gpuHandle);

    textureCache.erase(modelIter);
  }
}

ModelHandle TextureManager::assignModelHandle() {
  static std::random_device rd;
  static std::mt19937 mt(rd());
  constexpr uint64_t maxValue{std::numeric_limits<uint64_t>::max()};

  static std::uniform_int_distribution<uint64_t> rndHandleRange{1, maxValue};

  uint64_t handle{rndHandleRange(mt)};

  while (textureCache.contains(handle)) handle = rndHandleRange(mt);

  textureCache.emplace(handle, TextureEntry{});

  return handle;
}

void TextureManager::processTextureQueue() {
  if (textureCreationQueue.empty()) return;

  ThreadPool threadPool{desiredThreadCount};

  while (!textureCreationQueue.empty()) {
    auto& queueElement = textureCreationQueue.front();

    if (std::holds_alternative<fastgltf::Asset>(queueElement.source))
      genTexIDFromAsset(threadPool,
                        std::get<fastgltf::Asset>(queueElement.source),
                        queueElement.modelHandle, queueElement.parentPath);

    else if (std::holds_alternative<std::filesystem::path>(queueElement.source))
      genTexIDFromImage(threadPool, queueElement.modelHandle,
                        std::get<std::filesystem::path>(queueElement.source),
                        queueElement.aniso);

    else if (std::holds_alternative<std::vector<std::filesystem::path>>(
                 queueElement.source))
      genCubemapTexID(
          threadPool, queueElement.modelHandle,
          std::get<std::vector<std::filesystem::path>>(queueElement.source),
          queueElement.aniso);

    textureCreationQueue.pop();
  }
}

void TextureManager::genTexIDFromAsset(ThreadPool& threadPool,
                                       fastgltf::Asset& asset,
                                       ModelHandle handle,
                                       std::filesystem::path parentPath) {
  // Return not found
  auto handleIter = textureCache.find(handle);
  if (handleIter == textureCache.end()) return;

  // Get raw image data
  std::vector<std::filesystem::path> texturePaths{
      fastgltfLoader::getURIs(asset, parentPath)};
  ImageLoader imageLoader{threadPool, texturePaths};
  std::vector<LoadedImage> rawTextureData = imageLoader.getImageData();

  // Create key and resize
  TextureEntry& entry{handleIter->second};
  ++entry.refCount;
  entry.textureSets.resize(asset.materials.size());

  // Create map to avoid duplicate images
  std::unordered_map<unsigned int, GLuint> processedImageIndices;

  // Helper function
  auto getOrCreateTextureID = [&](const fastgltf::TextureInfo& textureInfo,
                                  bool sRGB) {
    auto [imageIndex, sampler] =
        fastgltfLoader::getTextureInfo(asset, textureInfo);

    auto [iter, inserted] = processedImageIndices.try_emplace(imageIndex, 0);

    if (inserted) {
      iter->second = textureUtilities::generateTextureID(
          rawTextureData[imageIndex], sampler, sRGB);
      entry.handlesInUse.insert(iter->second);
    }

    return iter->second;
  };

  auto& textureVec = entry.textureSets;

  // Process all materials in the gltf asset
  for (size_t i{0}; i < asset.materials.size(); ++i) {
    const auto& material = asset.materials[i];

    if (material.pbrData.baseColorTexture.has_value()) {
      auto& albedo = material.pbrData.baseColorTexture.value();
      textureVec[i].albedo = getOrCreateTextureID(albedo, true);
    }

    if (material.pbrData.metallicRoughnessTexture.has_value()) {
      auto& metallicRoughnessTexture =
          material.pbrData.metallicRoughnessTexture.value();
      textureVec[i].occlusionRoughnessMetallic =
          getOrCreateTextureID(metallicRoughnessTexture, false);
    }

    if (material.normalTexture.has_value()) {
      auto& normalTexture = material.normalTexture.value();
      textureVec[i].normal = getOrCreateTextureID(normalTexture, false);
    }
  }
}

void TextureManager::genTexIDFromImage(ThreadPool& threadPool,
                                       ModelHandle handle,
                                       const std::filesystem::path& imagePath,
                                       bool aniso) {
  auto handleIter = textureCache.find(handle);

  if (handleIter == textureCache.end()) return;

  ++handleIter->second.refCount;
  handleIter->second.textureSets.resize(1);

  ImageLoader imageLoader{threadPool, imagePath};
  std::vector<LoadedImage> imageData{imageLoader.getImageData()};

  if (imageData.size()) {
    GLuint texID{textureUtilities::generateTextureID(
        imageData[0],
        Sampler{GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, GL_CLAMP_TO_EDGE,
                GL_CLAMP_TO_EDGE},
        true, aniso)};

    handleIter->second.textureSets[0].albedo = texID;
    handleIter->second.handlesInUse.insert(texID);
  }
}

void TextureManager::genCubemapTexID(
    ThreadPool& threadPool, ModelHandle handle,
    const std::vector<std::filesystem::path>& imagePaths, bool aniso) {
  auto handleIter = textureCache.find(handle);

  if (handleIter == textureCache.end()) return;

  ++handleIter->second.refCount;
  handleIter->second.textureSets.resize(1);

  ImageLoader imageLoader{threadPool, imagePaths};
  std::vector<LoadedImage> loadedImages{imageLoader.getImageData()};

  GLuint texID{
      textureUtilities::generateCubemapTextureID(loadedImages, true, aniso)};

  handleIter->second.textureSets[0].cubemap = texID;
  handleIter->second.handlesInUse.insert(texID);
}

const std::vector<TextureSet>& TextureManager::borrowTextureSet(ModelHandle other) {

      auto textureSetIter = textureCache.find(other);

  if (textureSetIter == textureCache.end()) {
    std::cerr << "model handle not found" << std::endl;
    static const std::vector<TextureSet> empty;
    return empty;
  }

  ++textureSetIter->second.refCount;

  return textureSetIter->second.textureSets;
}
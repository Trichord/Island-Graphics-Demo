#include "texture_manager.h"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "fastgltf/types.hpp"
#include "fastgltf_loader.h"
#include "glad/glad.h"
#include "image_loader.h"
#include "texture_utilities.h"
#include "thread_pool.h"

constexpr unsigned int desiredThreadCount{6u};

void TextureManager::queueModelMaterials(
    const ModelHandle handle, fastgltf::Asset&& asset,
    const std::filesystem::path& parentPath) {
  textureCreationQueue.emplace(std::move(asset), handle, parentPath);
}

void TextureManager::queueCubeMap(
    const ModelHandle handle, std::vector<std::filesystem::path> imagePaths,
    const std::filesystem::path& parentPath, bool sRGB) {
  textureCreationQueue.emplace(std::move(imagePaths), handle, parentPath, sRGB);
}

// TODO: add default purple texture for missing texturesets
const std::vector<Material>& TextureManager::acquireUniqueTextureSet(
    ModelHandle modelHandle) const {
  auto textureSetIter = textureCache.find(modelHandle);

  if (textureSetIter == textureCache.end()) {
    std::cerr << "model handle not found" << std::endl;
    static const std::vector<Material> empty;
    return empty;
  }

  return textureSetIter->second.materials;
}

void TextureManager::releaseModel(ModelHandle handle) {
  auto modelIter{textureCache.find(handle)};

  if (modelIter == textureCache.end()) return;

  TextureEntry& entry{modelIter->second};
  entry.refCount--;

  if (entry.refCount == 0) {
    for (auto& gpuHandle : entry.textureHandles)
      glDeleteTextures(1, &gpuHandle);

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

    else if (std::holds_alternative<std::filesystem::path>(
                 queueElement.source)) {
      TextureConfig config{queueElement.sRGB, queueElement.requestAniso,
                           anisoLevel};
      genTexIDFromImage(threadPool, queueElement.modelHandle,
                        std::get<std::filesystem::path>(queueElement.source),
                        config);
    }

    else if (std::holds_alternative<std::vector<std::filesystem::path>>(
                 queueElement.source))
      genCubemapTexID(
          threadPool, queueElement.modelHandle,
          std::get<std::vector<std::filesystem::path>>(queueElement.source),
          queueElement.sRGB);

    textureCreationQueue.pop();
  }
}

void TextureManager::genTexIDFromAsset(ThreadPool& threadPool,
                                       fastgltf::Asset& asset,
                                       ModelHandle handle,
                                       std::filesystem::path parentPath) {
  // Return not found
  auto handleIter = textureCache.find(handle);
  if (handleIter == textureCache.end()) {
    std::cerr << "ERROR: Handle " << handle << " not found for model "
              << parentPath << std::endl;
    return;
  }

  // Get raw image data
  std::vector<std::filesystem::path> texturePaths{
      fastgltfLoader::getURIs(asset, parentPath)};
  ImageLoader imageLoader{threadPool, texturePaths};
  std::vector<LoadedImage> rawTextureData = imageLoader.getImageData();

  // Create key and resize
  TextureEntry& entry{handleIter->second};
  ++entry.refCount;
  entry.materials.resize(asset.materials.size());

  // Create map to avoid duplicate images
  std::unordered_map<unsigned int, GLuint> processedImageIndices;

  // Helper function
  auto getOrCreateTextureID = [&](const fastgltf::TextureInfo& textureInfo,
                                  bool sRGBRequested, bool anisoRequested) {
    auto [imageIndex, sampler] =
        fastgltfLoader::getTextureInfo(asset, textureInfo);

    auto [iter, inserted] = processedImageIndices.try_emplace(imageIndex, 0);

    if (inserted) {
      TextureConfig config{sRGBRequested, anisoRequested, anisoLevel};

      iter->second = textureUtilities::generateTextureID(
          rawTextureData[imageIndex], sampler, config, globalAnisoEnabled);
      entry.textureHandles.insert(iter->second);
    }

    return iter->second;
  };

  auto& textureVec = entry.materials;

  // Process all materials in the gltf asset
  for (size_t i{0}; i < asset.materials.size(); ++i) {
    const auto& material = asset.materials[i];

    if (material.pbrData.baseColorTexture.has_value()) {
      auto& albedo = material.pbrData.baseColorTexture.value();
      textureVec[i].albedo = getOrCreateTextureID(albedo, true, true);
    }

    if (material.pbrData.metallicRoughnessTexture.has_value()) {
      auto& metallicRoughnessTexture =
          material.pbrData.metallicRoughnessTexture.value();
      textureVec[i].occlusionRoughnessMetallic =
          getOrCreateTextureID(metallicRoughnessTexture, false, false);
    }

    if (material.normalTexture.has_value()) {
      auto& normalTexture = material.normalTexture.value();
      textureVec[i].normal = getOrCreateTextureID(normalTexture, false, false);
    }
  }
}

void TextureManager::genTexIDFromImage(ThreadPool& threadPool,
                                       ModelHandle handle,
                                       const std::filesystem::path& imagePath,
                                       TextureConfig textureConfig) {
  auto handleIter = textureCache.find(handle);

  if (handleIter == textureCache.end()) return;

  ++handleIter->second.refCount;
  handleIter->second.materials.resize(1);

  ImageLoader imageLoader{threadPool, imagePath};
  std::vector<LoadedImage> imageData{imageLoader.getImageData()};

  Sampler textureSampler{GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, GL_CLAMP_TO_EDGE,
                         GL_CLAMP_TO_EDGE};

  if (imageData.size()) {
    GLuint texID{textureUtilities::generateTextureID(
        imageData[0], textureSampler, textureConfig, globalAnisoEnabled)};

    handleIter->second.materials[0].albedo = texID;
    handleIter->second.textureHandles.insert(texID);
  }
}

void TextureManager::genCubemapTexID(
    ThreadPool& threadPool, ModelHandle handle,
    const std::vector<std::filesystem::path>& imagePaths, bool sRGB) {
  auto handleIter = textureCache.find(handle);

  if (handleIter == textureCache.end()) return;

  ++handleIter->second.refCount;
  handleIter->second.materials.resize(1);

  ImageLoader imageLoader{threadPool, imagePaths};
  std::vector<LoadedImage> loadedImages{imageLoader.getImageData()};

  GLuint texID{textureUtilities::generateCubemapTextureID(loadedImages, sRGB)};

  handleIter->second.materials[0].cubemap = texID;
  handleIter->second.textureHandles.insert(texID);
}

const std::vector<Material>& TextureManager::borrowTextureSet(
    ModelHandle other) {
  auto textureSetIter = textureCache.find(other);

  if (textureSetIter == textureCache.end()) {
    std::cerr << "model handle not found" << std::endl;
    static const std::vector<Material> empty;
    return empty;
  }

  ++textureSetIter->second.refCount;

  return textureSetIter->second.materials;
}

void TextureManager::setAnisoLevel(float level) {
  globalAnisoEnabled = true;
  anisoLevel = level;
}
#pragma once

#include "glad/glad.h"

#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <fastgltf/types.hpp>
#include <filesystem>

typedef uint64_t TextureHandle;
typedef uint64_t ModelHandle;

class TextureManager {
 public:
  struct TextureSet {
    GLuint albedo{0};
    GLuint occlusionRoughnessMetallic{0};
    GLuint normal{0};
    GLuint cubemap{0};
  };

  struct TextureEntry {
    TextureSet textureSet;
    int refCount{0};
  };

  const ModelHandle createModelTextureSet(
      const fastgltf::TextureInfo& textureInfo, bool sRGB = true);
  const ModelHandle createModelTextureSet(
      const std::filesystem::path& texturePath, bool sRGB = true);
  const ModelHandle createModelTextureSet(
      const std::vector<std::filesystem::path>& imagePaths, bool sRGB = true);

  const std::unordered_set<TextureHandle>& acquireTextureSet(
      ModelHandle modelHandle);
  void releaseModel(ModelHandle modelHandle);

 private:
  std::unordered_map<TextureHandle, TextureEntry> textureCache;
  std::unordered_map<ModelHandle, std::unordered_set<TextureHandle>>
      modelTextures;
  uint64_t createUniqueHandle() const;
  void deleteTextureIDs(TextureHandle textureHandle);
};
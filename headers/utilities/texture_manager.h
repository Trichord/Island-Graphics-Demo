#pragma once

#include "glad/glad.h"
#include "model.h"

#include <unordered_map>
#include <queue>
#include <variant>
#include <utility>
#include <vector>
#include <cstdint>
#include <fastgltf/types.hpp>
#include <filesystem>

typedef uint64_t ModelHandle;

struct TextureSet {
  GLuint albedo{0};
  GLuint occlusionRoughnessMetallic{0};
  GLuint normal{0};
  GLuint cubemap{0};
};

class TextureManager {
 public:
  const ModelHandle queueModelTextureSet(const ModelHandle handle,
                                         fastgltf::Scene&& scene,
                                         bool sRGB = true);
  const ModelHandle queueBaseImageTextureSet(const ModelHandle handle,
                                             std::filesystem::path texturePath,
                                             bool sRGB = true);
  const ModelHandle queueCubemapTextureSet(
      const ModelHandle handle, std::vector<std::filesystem::path> imagePaths,
      bool sRGB = true);
  const std::vector<TextureSet>& acquireTextureSet(
      ModelHandle modelHandle) const;
  void releaseModel(ModelHandle modelHandle);
  void processTextureQueue();

 private:
  struct TextureEntry {
    std::vector<TextureSet> textureSets;
    int refCount{0};
  };

  using TextureSourceType = std::variant < fastgltf::Scene,
        std::filesystem::path, std::vector<std::filesystem::path>>;

  struct TextureQueueElement {
    TextureSourceType source;
    ModelHandle modelHandle{0};
    bool sRGB{true};
  };

  uint64_t createUniqueHandle() const;
  std::unordered_map<ModelHandle, TextureEntry> textureCache;
  std::queue<TextureQueueElement> textureCreationQueue;

  void deleteTextureIDs(TextureSet textureSet);
};
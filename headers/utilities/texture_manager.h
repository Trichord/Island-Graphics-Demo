#pragma once

#include "glad/glad.h"
#include "thread_pool.h"

#include <unordered_map>
#include <unordered_set>
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
  void queueModelTextureSet(const ModelHandle handle, fastgltf::Asset&& asset,
                            const std::filesystem::path& parentPath);
  void queueBaseImageTextureSet(const ModelHandle handle,
                                std::filesystem::path texturePath);
  void queueCubemapTextureSet(const ModelHandle handle,
                              std::vector<std::filesystem::path> imagePaths,
                              const std::filesystem::path& parentPath = "./",
                              bool aniso = true);
  const std::vector<TextureSet>& acquireUniqueTextureSet(
      ModelHandle modelHandle) const;
  const std::vector<TextureSet>& borrowTextureSet(ModelHandle other);
  void releaseModel(ModelHandle modelHandle);
  void processTextureQueue();
  ModelHandle assignModelHandle();

 private:
  // Each entry in the vector corresponds to a material index
  struct TextureEntry {
    std::vector<TextureSet> textureSets;
    std::unordered_set<GLuint> handlesInUse;
    int refCount{0};
  };

  using TextureSourceType = std::variant<fastgltf::Asset, std::filesystem::path,
                                         std::vector<std::filesystem::path>>;

  struct TextureQueueElement {
    TextureSourceType source;
    ModelHandle modelHandle{0};
    std::filesystem::path parentPath{std::filesystem::current_path()};
    bool aniso{true};
  };

  std::unordered_map<ModelHandle, TextureEntry> textureCache;
  std::queue<TextureQueueElement> textureCreationQueue;

  void genTexIDFromAsset(ThreadPool& threadPool, fastgltf::Asset& asset,
                         ModelHandle handle, std::filesystem::path parentPath);
  void genTexIDFromImage(ThreadPool& threadPool, ModelHandle handle,
                         const std::filesystem::path& imagePath,
                         bool aniso = true);
  void genCubemapTexID(ThreadPool& threadPool, ModelHandle handle,
                       const std::vector<std::filesystem::path>& imagePaths,
                       bool aniso = true);
};
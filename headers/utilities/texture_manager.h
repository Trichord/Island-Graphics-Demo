#pragma once

#include "glad/glad.h"
#include "thread_pool.h"

#include "texture_units.h"
#include <fastgltf/types.hpp>
#include <filesystem>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

using ModelHandle = int;

struct Material
{
    GLuint albedo{0};
    GLuint occlusionRoughnessMetallic{0};
    GLuint normal{0};
    GLuint cubemap{0};
};

class TextureManager
{
  public:
    ModelHandle assignHandle();
    void releaseHandle(ModelHandle handle);

    const std::vector<Material> &borrowMaterials(ModelHandle other);
    const std::vector<Material> &getUniqueMaterials(ModelHandle modelHandle) const;
    void assignMaterial(ModelHandle handle, TextureUnit textureUnit, GLuint textureID, int materialIndex = 0);

    void processTextureQueue();
    void queueCubemap(const ModelHandle handle, std::vector<std::filesystem::path> images,
                      const std::filesystem::path &parentPath = "./", bool mipmaps = true, bool sRGB = true);
    void queueModel(const ModelHandle handle, fastgltf::Asset &&asset, const std::filesystem::path &parentPath);

    void setAnisoLevel(float anisoLevel);

  private:
    float anisoLevel{0.0f};
    bool globalAnisoEnabled{false};

    // Use signature of how textures were requested to decide on texture generation strategy
    using TextureSource = std::variant<fastgltf::Asset, std::vector<std::filesystem::path>>;

    // Each entry in the vector corresponds to a gltf material index. Materials vector can contain duplicate IDs
    // (multiple materials referencing the same texture atlas, etc.). Unordered set keeps track of which IDs need to be
    // freed on destruction.
    struct TextureEntry
    {
        int refCount{0};
        std::vector<Material> materials;
        std::unordered_set<GLuint> glTextureHandles;
    };

    struct TextureQueueElement
    {
        TextureSource source;
        ModelHandle handle{0};
        std::filesystem::path parentPath{std::filesystem::current_path()};

        bool mipmaps{true};
        bool sRGB{true};
        bool anisotropic{true};
    };

    std::queue<TextureQueueElement> textureCreationQueue;
    std::unordered_map<ModelHandle, TextureEntry> textureCache;

    // From a fastgltf::asset
    void textureFromAsset(ThreadPool &threadPool, fastgltf::Asset &asset, ModelHandle handle,
                          std::filesystem::path parentPath);

    // Create cubemap from vector of paths
    void cubemapFromPaths(ThreadPool &threadPool, ModelHandle handle, const std::vector<std::filesystem::path> &images,
                          bool mipMaps, bool sRGB);
};
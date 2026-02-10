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
    explicit TextureManager(ThreadPool &threadPool);

    ModelHandle assignHandle();
    void releaseHandle(ModelHandle handle);

    const std::vector<Material> &getMaterials(ModelHandle modelHandle) const;
    void assignMaterial(ModelHandle handle, Texture textureUnit, GLuint textureID, int materialIndex = 0);
    const std::vector<Material> &borrowMaterials(ModelHandle self, ModelHandle other,
                                                 std::optional<Texture> texture = std::nullopt, int materialIndex = 0);

    void loadCubemap(const ModelHandle handle, std::vector<std::filesystem::path> images,
                      const std::filesystem::path &parentPath = "./", bool mipmaps = true, bool sRGB = true);
    void loadModel(const ModelHandle handle, fastgltf::Asset &&asset, const std::filesystem::path &parentPath);

    void setAnisoLevel(float anisoLevel);
    void allModelsLoaded();

  private:
    float anisoLevel{0.0f};
    bool globalAnisoEnabled{false};

    ThreadPool& threadPool;

    // Use signature of how textures were requested to decide on texture generation strategy
    using TextureSource = std::variant<fastgltf::Asset, std::vector<std::filesystem::path>>;

    // Each entry in the vector corresponds to a gltf material index. Materials vector can contain duplicate IDs
    // (multiple materials referencing the same texture atlas, etc.). Unordered set keeps track of which IDs need to be
    // freed on destruction. Lenders keeps track of
    struct TextureEntry
    {
        int refCount{1};
        std::vector<Material> materials;
        std::unordered_set<GLuint> glTextureHandles;
        ModelHandle lender{-1};
    };

    std::unordered_map<ModelHandle, TextureEntry> textureCache;

    // From a fastgltf::asset
    void textureFromAsset(fastgltf::Asset &asset, ModelHandle handle,
                          std::filesystem::path parentPath);

    // Create cubemap from vector of paths
    void cubemapFromPaths(ModelHandle handle, const std::vector<std::filesystem::path> &images,
                          bool mipMaps, bool sRGB);

    void freeResources(TextureEntry &textureEntry);
    void setMaterial(TextureEntry &self, TextureEntry &other, Texture texture, int index);
};
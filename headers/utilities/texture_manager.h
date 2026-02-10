#pragma once

#include <fastgltf/types.hpp>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "glad/glad.h"
#include "texture_units.h"
#include "thread_pool.h"

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
    void setMaterial(ModelHandle handle, Texture texture, GLuint textureID, int materialIndex = 0);
    const std::vector<Material> &borrowAssignMaterials(ModelHandle self, ModelHandle other,
                                                       std::optional<Texture> texture = std::nullopt,
                                                       int materialIndex = 0);

    void loadCubemap(const ModelHandle handle, std::vector<std::filesystem::path> images,
                     const std::filesystem::path &parentPath = "./", bool mipmaps = true, bool sRGB = true);
    void loadModel(const ModelHandle handle, fastgltf::Asset &&asset, const std::filesystem::path &parentPath);

    void setAnisoLevel(float anisoLevel);
    void allModelsLoaded();

  private:
    float anisoLevel{0.0f};
    bool globalAnisoEnabled{false};

    ThreadPool &threadPool;

    // Each entry in the vector corresponds to a gltf material index. Materials vector can contain duplicate IDs
    // (multiple materials referencing the same texture atlas, etc.). Unordered set keeps track of which IDs need to be
    // freed on destruction. Lenders keeps track of who the textures were borrowed from.
    struct ModelResources
    {
        int refCount{1};
        std::vector<Material> materials;
        std::unordered_set<GLuint> glTextureHandles;
        ModelHandle lender{-1};
    };

    std::unordered_map<ModelHandle, ModelResources> textureCache;

    // From a fastgltf::asset
    void textureFromAsset(fastgltf::Asset &asset, ModelHandle handle, std::filesystem::path parentPath);

    // Create cubemap from vector of paths
    void cubemapFromPaths(ModelHandle handle, const std::vector<std::filesystem::path> &images, bool mipmaps,
                          bool sRGB);

    void freeResources(ModelResources &resources);
    void setMaterial(ModelResources &self, ModelResources &other, Texture texture, int index);
};
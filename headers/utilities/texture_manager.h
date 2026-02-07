#pragma once

#include "glad/glad.h"
#include "texture_utilities.h"
#include "thread_pool.h"

#include "texture_units.h"
#include <fastgltf/types.hpp>
#include <filesystem>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

typedef int ModelHandle;

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
    void setAnisoLevel(float anisoLevel);
    void queueModelMaterials(const ModelHandle handle, fastgltf::Asset &&asset,
                             const std::filesystem::path &parentPath);
    void queueCubeMap(const ModelHandle handle, std::vector<std::filesystem::path> imagePaths,
                      const std::filesystem::path &parentPath = "./", bool mipMaps = true, bool sRGB = true);
    const std::vector<Material> &getUniqueMaterials(ModelHandle modelHandle) const;
    const std::vector<Material> &borrowMaterials(ModelHandle other);
    void releaseModel(ModelHandle handle);
    void processTextureQueue();
    ModelHandle assignModelHandle();
    void assignMaterial(ModelHandle handle, TextureUnit textureUnit, GLuint textureID, int materialIndex = 0);

  private:
    float anisoLevel{0.0f};
    bool globalAnisoEnabled{false};

    // Each entry in the vector corresponds to a gltf material index
    struct TextureEntry
    {
        std::vector<Material> materials;
        std::unordered_set<GLuint> textureHandles;
        int refCount{0};
    };

    using TextureSourceType = std::variant<fastgltf::Asset, std::filesystem::path, std::vector<std::filesystem::path>>;

    struct TextureQueueElement
    {
        TextureSourceType source;
        ModelHandle modelHandle{0};
        std::filesystem::path parentPath{std::filesystem::current_path()};

        bool mipMaps{true};
        bool sRGB{true};
        bool requestAniso{true};
    };

    std::unordered_map<ModelHandle, TextureEntry> textureCache;
    std::queue<TextureQueueElement> textureCreationQueue;

    void genTexIDFromAsset(ThreadPool &threadPool, fastgltf::Asset &asset, ModelHandle handle,
                           std::filesystem::path parentPath);
    void genTexIDFromImage(ThreadPool &threadPool, ModelHandle handle, const std::filesystem::path &imagePath,
                           TextureConfig textureConfig);
    void genCubemapTexID(ThreadPool &threadPool, ModelHandle handle,
                         const std::vector<std::filesystem::path> &imagePaths, bool mipMaps, bool sRGB);
};
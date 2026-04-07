#pragma once

#include <fastgltf/types.hpp>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "glad/glad.h"
#include "settings_window.h"
#include "textures.h"
#include "thread_pool.h"

using ModelHandle = int;

class TextureManager
{
  public:
    TextureManager(ThreadPool &threadPool, const GraphicsSettings &settings);
    ~TextureManager();

    ModelHandle assignHandle();

    void setCubemap(ModelHandle handle, GLuint textureID);
    const std::vector<Material> &getMaterials(ModelHandle modelHandle) const;

    void borrowMaterials(ModelHandle self, ModelHandle other);
    void borrowCubemaps(ModelHandle self, ModelHandle other);

    void loadCubemap(const ModelHandle handle, std::vector<std::filesystem::path> images,
                     const std::filesystem::path &parentPath = "./", bool mipmaps = true, bool sRGB = true);
    void loadModel(const ModelHandle handle, fastgltf::Asset &&asset, const std::filesystem::path &parentPath);

  private:
    struct TextureArray
    {
        const int width{0};
        const GLenum internalFormat{GL_SRGB8};
        std::vector<GLuint> textureIDs;
    };

    std::array<TextureArray, 8> textureArrays_{{{4096, GL_RGB8},
                                               {4096, GL_SRGB8_ALPHA8},
                                                {2048, GL_RGB8},
                                               {2048, GL_SRGB8_ALPHA8},
                                                {1024, GL_RGB8},
                                               {1024, GL_SRGB8_ALPHA8},
                                                {512, GL_RGB8},
                                                {256, GL_RGB8}}};

    float globalAnisotropicDegree_{0.0f};
    bool globalAnisoEnabled_{false};

    ThreadPool &threadPool_;
    std::unordered_map<ModelHandle, std::vector<Material>> materialLookup_;
    std::unordered_set<GLuint> cubemapIDs_;

    // From a fastgltf::asset
    void textureFromAsset(fastgltf::Asset &asset, ModelHandle handle, std::filesystem::path parentPath);

    // Create cubemap from vector of paths
    void cubemapFromPaths(ModelHandle handle, const std::vector<std::filesystem::path> &images, bool mipmaps,
                          bool sRGB);

    SamplerArrayEntry assignToTextureArray(Texture& texture);
    void freeArrayTextures(SamplerArrayEntry &entry);
};
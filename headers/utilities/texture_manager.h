#pragma once

#include <array>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <unordered_map>
#include <vector>

#include "glad/glad.h"
#include "graphics_settings.h"
#include "render_target.h"
#include "textures.h"
#include "thread_pool.h"

// To do: Verify process for generating mipmaps. Texture utilities may not need to take in anything other than sRGB and
// aniso. Allow or deny setting of dynamic cubemap internal format (through constructor).

using ModelHandle = int;

class TextureManager
{
  public:
    TextureManager(ThreadPool &threadPool, const GraphicsSettings &settings, int reflectionResolution);
    ~TextureManager();

    ModelHandle assignHandle();

    // Static loaded textures
    void loadCubemapTextures(const ModelHandle handle, std::vector<std::filesystem::path> images,
                             const std::filesystem::path &parentPath = "./", bool mipmaps = true);
    void loadModelTextures(const ModelHandle handle, fastgltf::Asset &&asset, const std::filesystem::path &parentPath);

    // Dynamic texture borrowing
    void borrowCubemaps(ModelHandle self, ModelHandle other);
    void borrowMaterials(ModelHandle self, ModelHandle other);

    // Accessors and mutators
    GLenum getReflectionColorFormat() const;
    const std::vector<Material> &getMaterials(ModelHandle modelHandle) const;
    void setCubemap(ModelHandle handle, RenderTarget &renderTarget, SamplerArrayEntry arrayEntry);

    // Bindless array texture setup
    void buildTextures();
    void buildCubemaps();
    SamplerArrayEntry reserveCubemaps(int numCubemaps, int resolution, GLenum internalFormat);

    // Lock in GPU resources
    void texturesReserved();
    void cubemapsReserved();

  private:
    struct ArrayTexture
    {
        GLuint id{0};
        int width{0};
        int count{0};
        GLenum internalFormat{GL_SRGB8};
    };

    struct PendingTexture
    {
        GLuint id{0};
        SamplerArrayEntry arrayEntry;
    };

    std::array<ArrayTexture, 8> arrayTextures_{{{.width = 4096, .internalFormat = GL_RGB8},
                                                {.width = 4096, .internalFormat = GL_SRGB8},
                                                {.width = 2048, .internalFormat = GL_RGB8},
                                                {.width = 2048, .internalFormat = GL_SRGB8},
                                                {.width = 2048, .internalFormat = GL_SRGB8_ALPHA8},
                                                {.width = 1024, .internalFormat = GL_RGB8},
                                                {.width = 1024, .internalFormat = GL_SRGB8},
                                                {.width = 512, .internalFormat = GL_RGB8}}};

    // Third slot empty to handle run-time selection of reflection cubemaps
    std::array<ArrayTexture, 3> cubemapArrayTextures_{{
        {.width = 4096, .internalFormat = GL_SRGB8},
        {.width = 2048, .internalFormat = GL_SRGB8},
    }};

    GLenum dynamicCubemapInternalFormat_{GL_SRGB8};

    // Textures to be uploaded
    std::vector<PendingTexture> pendingCubemaps_;
    std::vector<PendingTexture> pending2dTextures_;

    ThreadPool &threadPool_;
    float globalAnisotropicDegree_{0.0f};
    std::unordered_map<ModelHandle, std::vector<Material>> materialLookup_;

    bool textures2dAllocated_{false};
    bool texturesCubemapAllocated_{false};

    // Textures from loaded models
    void texturesFromAsset(fastgltf::Asset &asset, ModelHandle handle, std::filesystem::path parentPath);
    void cubemapFromPaths(ModelHandle handle, const std::vector<std::filesystem::path> &images, bool mipmaps);

    // Helpers for assigning to texture buckets
    SamplerArrayEntry assignToTextureArray(Texture &texture);
    SamplerArrayEntry assignToCubeArray(Texture &texture);
};
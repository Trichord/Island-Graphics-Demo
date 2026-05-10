#pragma once

#include <array>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <unordered_map>
#include <vector>

#include "glad/glad.h"
#include "gli/texture.hpp"
#include "graphics_settings.h"
#include "render_target.h"
#include "textures.h"
#include "thread_pool.h"

// To do: Verify process for generating mipmaps. Texture utilities may not need to take in anything other than sRGB and
// aniso. Allow or deny setting of dynamic cubemap internal format (through constructor).

using Handle = int;

class TextureManager
{
  public:
    TextureManager(ThreadPool &threadPool, const GraphicsSettings &settings, int reflectionResolution);
    ~TextureManager();

    Handle assignHandle();

    // Static loaded textures
    void loadModelTextures(const Handle handle, fastgltf::Asset &&asset, const std::filesystem::path &parentPath,
                           const std::optional<Handle> borrowHandle);
    void loadCubemapTextures(const Handle handle, const std::filesystem::path &cubePath,
                             std::vector<int> &materialIndices);

    GLenum getReflectionColorFormat() const;
    const std::vector<Material> &getMaterials(Handle handle) const;

    // Dynamic cubemap borrowing
    void borrowCubemaps(Handle self, Handle other);
    void setCubemap(Handle handle, std::vector<int> overrideIndices, RenderTarget &renderTarget);

    void buildAllTextures();
    void reserveCubemaps(int numCubemaps, int resolution, GLenum internalFormat);

  private:
    struct ArrayTexture
    {
        GLuint id{0};
        int size{0};
        int count{0};
        GLenum internalFormat{GL_COMPRESSED_RGBA_BPTC_UNORM};
    };

    struct DeferredTexture
    {
        Handle handle{0};
        std::filesystem::path path;

        Material::Type type;
        SamplerArrayEntry entry;

        int size{0};
        bool textureRepeat{true};
        std::vector<int> materialIndices;
        GLenum internalFormat{GL_COMPRESSED_RGBA_BPTC_UNORM};
    };

    std::array<ArrayTexture, 10> arrayTextures_{{{.size = 4096, .internalFormat = GL_COMPRESSED_RG_RGTC2},
                                                 {.size = 4096, .internalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM},
                                                 {.size = 4096, .internalFormat = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM},

                                                 {.size = 2048, .internalFormat = GL_COMPRESSED_RG_RGTC2},
                                                 {.size = 2048, .internalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM},
                                                 {.size = 2048, .internalFormat = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM},

                                                 {.size = 1024, .internalFormat = GL_COMPRESSED_RG_RGTC2},
                                                 {.size = 1024, .internalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM},
                                                 {.size = 1024, .internalFormat = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM},

                                                 {.size = 512, .internalFormat = GL_COMPRESSED_RG_RGTC2}}};

    // Third slot (determined at runtime) handles dynamically generated reflection cubemaps
    std::array<ArrayTexture, 3> cubemapArrayTextures_{{
        {.size = 4096, .internalFormat = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM},
        {.size = 2048, .internalFormat = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM},
    }};

    bool texturesBuilt_{false};
    float globalAnisotropicDegree_{0.0f};
    GLenum dynamicCubemapInternalFormat_{GL_SRGB8};

    ThreadPool &threadPool_;
    std::unordered_map<Handle, std::vector<Material>> materialLookup_;

    // Textures to be uploaded
    std::vector<DeferredTexture> deferredCubemaps_;
    std::vector<DeferredTexture> deferredTextures_;

    // Borrows to be resolved after textures are done
    std::vector<std::pair<Handle, Handle>> pendingBorrows_;

    // Textures from disk
    std::vector<gli::texture> loadTexturesFromPool(const std::vector<DeferredTexture> &textures);
    void texturesFromAsset(fastgltf::Asset &asset, Handle handle, std::filesystem::path parentPath);
    void resolveBorrows();

    // Deferred texture processing  
    bool validateTexture(const gli::texture &texture);
    void createTextureLayers(auto &arrayTextures, bool cubemaps);
    void promoteToSRGB(DeferredTexture& deferred, const gli::texture& texture);
    void allocateAndFillArray(auto &array, std::vector<DeferredTexture> &deferred, bool cubemaps);
    void insertTexture(auto &array, const DeferredTexture &deferred, const gli::texture &texture);

    // Helpers for assigning to texture buckets
    ArrayTexture &findArrayEntry(auto &arrayTextures, int size, GLenum internalFormat);
    SamplerArrayEntry assignToTextureArray(auto &arrayTextures, int size, GLenum internalFormat);

    // Private helper for material lookups. Mirrors the public API, except its not const
    std::vector<Material> &findMaterials(Handle handle);
};
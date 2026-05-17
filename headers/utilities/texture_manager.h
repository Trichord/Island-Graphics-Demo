#pragma once

#include <fastgltf/types.hpp>
#include <filesystem>
#include <span>
#include <unordered_map>
#include <vector>

#include "array_texture.h"
#include "asset_loader.h"
#include "glad/glad.h"
#include "graphics_settings.h"
#include "handle.h"
#include "ktx_texture.h"
#include "render_target.h"
#include "textures.h"
#include "thread_pool.h"

// Texture units. Easier to keep it here with the arrays
namespace TextureUnit
{
static constexpr int array2DBase{0};
static constexpr int arrayCubemapBase{10};
static constexpr int shadow{13};
} // namespace TextureUnit

class TextureManager
{
  public:
    TextureManager(ThreadPool &threadPool, const GraphicsSettings &settings);

    Handle assignHandle();

    // Add a gLTF model
    void loadModelTextures(const Handle handle, const fastgltf::Asset &asset, const std::filesystem::path &parentPath);
    void loadCubemapTextures(const Handle handle, const std::filesystem::path &cubePath);

    GLenum getReflectionColorFormat() const;
    std::span<const Material> getMaterials(Handle handle) const;

    void reserveCubemaps(int numCubemaps, int resolution, GLenum internalFormat);

    // Borrow a cubemap after the scene has been built
    void borrowCubemaps(Handle self, Handle other);
    void setCubemap(Handle handle, std::vector<int> overrideIndices, RenderTarget &renderTarget);

    void buildAllTextures();

  private:
    struct ArrayReference
    {
        ArrayTexture &array;
        int index{0};
    };

    struct MaterialRange
    {
        std::size_t begin{0};
        std::size_t end{0};
    };

    std::vector<ArrayTexture> arrayTextures_;
    std::vector<ArrayTexture> arrayCubemaps_;

    bool texturesBuilt_{false};
    float globalAnisotropicDegree_{0.0f};
    GLenum dynamicCubemapInternalFormat_{GL_SRGB8};

    ThreadPool &threadPool_;
    std::vector<Material> materials_;
    std::unordered_map<Handle, MaterialRange> materialLookup_;

    // Cached filenames for sharing atlases. Paths are ignored - image filenames must be unique across the project
    std::unordered_map<std::filesystem::path, SamplerArrayIndex> filepathCache_;

    // Textures to be uploaded
    std::vector<AssetLoader::DeferredTexture> deferredTextures_;

    // Textures from disk
    std::vector<KTXTexture> loadTexturesFromPool(const std::vector<AssetLoader::DeferredTexture> &textures);

    // Helpers for assigning to texture buckets
    ArrayReference findCubemapArray(int size, GLenum internalFormat);
    ArrayReference findArray(const AssetLoader::DeferredTexture &deferred, const KTXTexture &texture);

    void countTexture(const AssetLoader::DeferredTexture &deferred, const KTXTexture &texture);
    void insertTexture(const AssetLoader::DeferredTexture &deferred, const KTXTexture &texture);

    // Private helper for material lookups. Mirrors the public API, except its not const
    std::span<Material> findMaterials(Handle handle);
    void addMaterials(Handle handle, std::size_t size);

    void validateState();
    void assignFallbacks();
};
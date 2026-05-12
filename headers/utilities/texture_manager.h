#pragma once

#include "handle.h"
#include <fastgltf/types.hpp>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "array_texture.h"
#include "asset_loader.h"
#include "glad/glad.h"
#include "gli/texture.hpp"
#include "graphics_settings.h"
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
    TextureManager(ThreadPool &threadPool, const GraphicsSettings &settings, int reflectionResolution);

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
    struct ArrayReference
    {
        ArrayTexture &array;
        int index{0};
    };

    std::vector<ArrayTexture> arrayTextures_;
    std::vector<ArrayTexture> arrayCubemaps_;

    bool texturesBuilt_{false};
    float globalAnisotropicDegree_{0.0f};
    GLenum dynamicCubemapInternalFormat_{GL_SRGB8};

    ThreadPool &threadPool_;
    std::unordered_map<Handle, std::vector<Material>> materialLookup_;

    // Textures to be uploaded
    std::vector<AssetLoader::DeferredTexture> deferredTextures_;

    // Borrows to be resolved after textures are done
    std::vector<std::pair<Handle, Handle>> pendingBorrows_;

    // Textures from disk
    std::vector<gli::texture> loadTexturesFromPool(const std::vector<AssetLoader::DeferredTexture> &textures);

    // Helpers for assigning to texture buckets
    ArrayReference findCubemapArray(int size, GLenum internalFormat);
    ArrayReference findArray(const AssetLoader::DeferredTexture &deferred, const gli::texture &texture);

    void countTexture(const AssetLoader::DeferredTexture &deferred, const gli::texture &texture);
    void insertTexture(const AssetLoader::DeferredTexture &deferred, const gli::texture &texture);

    // Private helper for material lookups. Mirrors the public API, except its not const
    std::vector<Material> &findMaterials(Handle handle);

    void resolveBorrows();
    void validateState();
};
#pragma once

#include <fastgltf/types.hpp>
#include <filesystem>
#include <glad/glad.h>
#include <span>
#include <unordered_map>
#include <vector>

#include "application/graphics_settings.h"
#include "renderer/render_target.h"
#include "renderer/textures.h"
#include "scene/handle.h"
#include "utilities/array_texture.h"
#include "utilities/asset_loader.h"
#include "utilities/ktx_texture.h"
#include "utilities/thread_pool.h"

class TextureManager
{
  public:
    TextureManager(ThreadPool &threadPool, const GraphicsSettings &settings);

    Handle assignHandle();

    // Add a gLTF model
    void loadModelTextures(const Handle handle, const fastgltf::Asset &asset, const std::filesystem::path &parentPath);
    void loadCubemapTextures(const Handle handle, const std::filesystem::path &cubePath);

    void addDefaultMaterial(const Handle handle);
    std::span<const Material> getMaterials(Handle handle) const;

    void reserveCubemaps(std::size_t numCubemaps, int resolution, GLenum internalFormat);

    // Borrow a cubemap after the scene has been built
    void borrowCubemaps(Handle self, Handle other);
    void setCubemap(Handle handle, std::vector<int> overrideIndices, RenderTarget &renderTarget);

    void buildAllTextures();
    bool allTexturesBuilt() const;

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
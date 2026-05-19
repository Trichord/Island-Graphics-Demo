#include "texture_manager.h"

#include <filesystem>
#include <iostream>
#include <numeric>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "array_texture.h"
#include "asset_loader.h"
#include "fastgltf/types.hpp"
#include "glad/glad.h"
#include "graphics_settings.h"
#include "handle.h"
#include "ktx_texture.h"
#include "render_target.h"
#include "texture_loader.h"
#include "texture_utilities.h"
#include "textures.h"
#include "thread_pool.h"

namespace
{
void missingHandleError(Handle handle)
{
    std::cerr << "ERROR: missing handle for " << handle << " .Could not get materials " << std::endl;
    throw std::runtime_error("Missing handle");
}
} // namespace

TextureManager::TextureManager(ThreadPool &threadPool, const GraphicsSettings &settings)
    : threadPool_{threadPool}, globalAnisotropicDegree_{static_cast<float>(settings.anisotropicFiltering)}
{
    /*
     * Fallback textures only for the smallest and easiest format in both sets. All regular textures get a 512x512 and
     * all cubemaps get an SRGB cubemap. ArrayTexture fallback support for different internal formats is minimal.
     */

    arrayTextures_.reserve(10);

    arrayTextures_.emplace_back(ArrayTexture{4096, 4096, GL_COMPRESSED_RG_RGTC2});
    arrayTextures_.emplace_back(ArrayTexture{4096, 4096, GL_COMPRESSED_RGBA_BPTC_UNORM});
    arrayTextures_.emplace_back(ArrayTexture{4096, 4096, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM});

    arrayTextures_.emplace_back(ArrayTexture{2048, 2048, GL_COMPRESSED_RG_RGTC2});
    arrayTextures_.emplace_back(ArrayTexture{2048, 2048, GL_COMPRESSED_RGBA_BPTC_UNORM});
    arrayTextures_.emplace_back(ArrayTexture{2048, 2048, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM});

    arrayTextures_.emplace_back(ArrayTexture{1024, 1024, GL_COMPRESSED_RG_RGTC2});
    arrayTextures_.emplace_back(ArrayTexture{1024, 1024, GL_COMPRESSED_RGBA_BPTC_UNORM});
    arrayTextures_.emplace_back(ArrayTexture{1024, 1024, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM});

    arrayTextures_.emplace_back(ArrayTexture{512, 512, GL_COMPRESSED_RG_RGTC2, true});

    arrayCubemaps_.reserve(3);

    arrayCubemaps_.emplace_back(ArrayTexture{4096, 4096, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM});
    arrayCubemaps_.emplace_back(ArrayTexture{2048, 2048, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM});
    arrayCubemaps_.emplace_back(ArrayTexture{settings.reflectionResolution, settings.reflectionResolution,
                                             dynamicCubemapInternalFormat_, true});
}

Handle TextureManager::assignHandle()
{
    static Handle nextHandle{1};

    Handle handle{nextHandle++};

    materialLookup_[handle];

    return handle;
}

void TextureManager::loadModelTextures(const Handle handle, const fastgltf::Asset &asset,
                                       const std::filesystem::path &parentPath)
{
    addMaterials(handle, asset.materials.size());

    const std::vector<AssetLoader::DeferredTexture> loaded{AssetLoader::texturesFromAsset(asset, parentPath, handle)};
    deferredTextures_.insert(deferredTextures_.end(), loaded.begin(), loaded.end());
}

bool TextureManager::allTexturesBuilt() const
{
    return texturesBuilt_;
}

void TextureManager::loadCubemapTextures(const Handle handle, const std::filesystem::path &cubePath)
{
    auto materials = findMaterials(handle);
    std::vector<int> materialIndices;

    if (materials.empty())
    {
        // Handle edge case where we specifically loaded a cubemap but the model has no materials.
        materialIndices = {0};
        addMaterials(handle, 1);
    }
    else
    {
        // Every model gets one cubemap
        materialIndices.resize(materials.size());
        std::iota(materialIndices.begin(), materialIndices.end(), 0);
    }

    deferredTextures_.emplace_back(AssetLoader::DeferredTexture{
        .handle = handle,
        .path = cubePath,
        .textureRepeat = false,
        .materialIndices = materialIndices,
        .type = Material::cubemap,
    });
}

// Returns the internal color format used for dynamic cubemaps
GLenum TextureManager::getReflectionColorFormat() const
{
    return dynamicCubemapInternalFormat_;
}

std::span<const Material> TextureManager::getMaterials(Handle handle) const
{
    auto iter = materialLookup_.find(handle);

    if (iter == materialLookup_.end())
        missingHandleError(handle);

    MaterialRange range{iter->second};

    return std::span<const Material>{materials_}.subspan(range.begin, range.end - range.begin);
}

// Reserve space in the ArrayTextures
void TextureManager::reserveCubemaps(std::size_t numCubemaps, int resolution, GLenum internalFormat)
{
    if (texturesBuilt_)
    {
        std::cerr << "ERROR: Cannot reserve after TextureManager::buildAllTextures() called" << std::endl;
        throw std::logic_error("Late function call");
    }

    ArrayReference arrayRef{findCubemapArray(resolution, internalFormat)};
    arrayRef.array.addCapacity(numCubemaps);
}

// Unconditionally borrow cubemaps from another model, overwriting existing ArrayTexture indices
void TextureManager::borrowCubemaps(Handle self, Handle other)
{
    if (!texturesBuilt_)
    {
        std::cerr << "ERROR: Cannot borrow before TextureManager::buildAllTextures() called" << std::endl;
        throw std::logic_error("Early function call");
    }

    auto selfMaterials{findMaterials(self)};
    auto otherMaterials{findMaterials(other)};

    if (selfMaterials.size() != otherMaterials.size())
        std::cerr << "WARNING: " << self << " borrowing mismatched material size from " << other << std::endl;

    if (selfMaterials.size() > otherMaterials.size())
    {
        std::cerr << "ERROR: " << self << " materials (" << selfMaterials.size() << ") larger than " << other
                  << " materials (" << otherMaterials.size() << "), uninitialized materials" << std::endl;
        throw std::runtime_error("Material borrow size error");
    }

    for (std::size_t i{0}; i < selfMaterials.size() && i < otherMaterials.size(); ++i)
    {
        SamplerArrayIndex &selfArray{selfMaterials[i].layers[Material::cubemap]};
        SamplerArrayIndex &otherArray{otherMaterials[i].layers[Material::cubemap]};
        selfArray = otherArray;
    }
}

// Unconditionally sets a cubemap for this model, overwriting existing ArrayTexture indices
void TextureManager::setCubemap(Handle handle, std::vector<int> overrideIndices, RenderTarget &renderTarget)
{
    const int resolution{renderTarget.getWidth()};
    ArrayReference arrayRef{findCubemapArray(resolution, dynamicCubemapInternalFormat_)};

    int layer{arrayRef.array.insert(renderTarget)};
    SamplerArrayIndex arrayIndex{.array = arrayRef.index, .layer = layer};

    arrayRef.array.generateMipmaps();

    std::span<Material> materials{findMaterials(handle)};

    for (int materialIndex : overrideIndices)
    {
        if (materialIndex >= static_cast<int>(materials.size()))
            continue;

        Material &material{materials[materialIndex]};
        SamplerArrayIndex &entry{material.layers[Material::cubemap]};
        entry = arrayIndex;
    }
}

void TextureManager::buildAllTextures()
{
    if (deferredTextures_.empty())
    {
        texturesBuilt_ = true;
        return;
    }

    validateState();

    // Enforce strict order. This function should get called once and only once
    texturesBuilt_ = true;

    std::cout << "Loading Textures... ";

    // If models share an atlas and have deleted the file, correct array indexes will be assigned later from the cached
    // file names, but the texture itself will be empty.
    std::vector<KTXTexture> textures{loadTexturesFromPool(deferredTextures_)};

    std::cout << "Done" << std::endl;

    // Count all the buckets and pre-size the ArrrayTextures since they're immutable afterwards
    for (std::size_t i{0}; i < deferredTextures_.size(); ++i)
        countTexture(deferredTextures_[i], textures[i]);

    // Lock in ArrayTexture sizes
    for (ArrayTexture &array : arrayTextures_)
        array.allocate(false, true, true, globalAnisotropicDegree_);

    for (ArrayTexture &array : arrayCubemaps_)
        array.allocate(true, false, true, globalAnisotropicDegree_);

    // Insert all textures
    for (std::size_t i{0}; i < deferredTextures_.size(); ++i)
        insertTexture(deferredTextures_[i], textures[i]);

    // Bind all units
    for (int i{0}; i < static_cast<int>(arrayTextures_.size()); ++i)
        arrayTextures_[i].bindTextureUnit(TextureUnit::array2DBase + i);

    for (int i{0}; i < static_cast<int>(arrayCubemaps_.size()); ++i)
        arrayCubemaps_[i].bindTextureUnit(TextureUnit::arrayCubemapBase + i);

    assignFallbacks();
    filepathCache_.clear();
    deferredTextures_.clear();
}

std::vector<KTXTexture> TextureManager::loadTexturesFromPool(const std::vector<AssetLoader::DeferredTexture> &textures)
{
    std::vector<std::filesystem::path> paths;

    paths.reserve(textures.size());

    for (const AssetLoader::DeferredTexture &texture : textures)
        paths.emplace_back(texture.path);

    TextureLoader loader{threadPool_, paths};

    return loader.getTextures();
}

void TextureManager::countTexture(const AssetLoader::DeferredTexture &deferred, const KTXTexture &texture)
{
    if (!texture)
        return;

    ArrayReference arrayRef{findArray(deferred, texture)};
    arrayRef.array.addCapacity(1);
}

// Insert into a ArrayTexture. Texture atlases can be shared across models
void TextureManager::insertTexture(const AssetLoader::DeferredTexture &deferred, const KTXTexture &texture)
{
    std::span<Material> materials{findMaterials(deferred.handle)};
    auto fileCacheIter = filepathCache_.find(deferred.path.filename());

    // Texture has been seen, re-use the array indexing
    if (fileCacheIter != filepathCache_.end())
    {
        SamplerArrayIndex other = fileCacheIter->second;
        for (int idx : deferred.materialIndices)
        {
            Material &material{materials[idx]};
            material.layers[deferred.type] = other;
        }

        return;
    }

    if (!texture)
    {
        std::cerr << "WARNING: Empty texture " << deferred.path << std::endl;
        return;
    }

    // New texture
    ArrayReference arrayRef{findArray(deferred, texture)};
    int layer{arrayRef.array.insert(texture)};
    SamplerArrayIndex arrayIndex{.array = arrayRef.index, .layer = layer};

    for (int idx : deferred.materialIndices)
    {
        Material &material{materials[idx]};
        material.layers[deferred.type] = arrayIndex;
    }

    filepathCache_.emplace(deferred.path.filename(), arrayIndex);
}

TextureManager::ArrayReference TextureManager::findArray(const AssetLoader::DeferredTexture &deferred,
                                                         const KTXTexture &texture)
{
    const bool isCubemap{deferred.type == Material::cubemap};
    std::vector<ArrayTexture> &arrayTextures{isCubemap ? arrayCubemaps_ : arrayTextures_};

    GLenum internalFormat{TextureUtilities::vkFormatToGL(texture->vkFormat)};
    int width{static_cast<int>(texture->baseWidth)};
    int height{static_cast<int>(texture->baseHeight)};

    for (int i{0}; i < static_cast<int>(arrayTextures.size()); ++i)
    {
        ArrayTexture &array{arrayTextures[i]};

        if (array.getWidth() == width && array.getHeight() == height && array.getInternalFormat() == internalFormat)
        {
            return {array, i};
        }
    }

    std::cerr << "ERROR: No array for " << width << "x" << height << ", format: " << internalFormat
              << ", cubemap = " << isCubemap << std::endl;
    throw std::runtime_error("Missing ArrayTexture");
}

TextureManager::ArrayReference TextureManager::findCubemapArray(int size, GLenum internalFormat)
{
    for (int i{0}; i < static_cast<int>(arrayCubemaps_.size()); ++i)
        if (arrayCubemaps_[i].getWidth() == size && arrayCubemaps_[i].getInternalFormat() == internalFormat)
            return {arrayCubemaps_[i], i};

    std::cerr << "ERROR: No cubemap array for " << size << "x" << size << ", format: " << internalFormat << std::endl;
    throw std::runtime_error("Missing ArrayTexture");
}

void TextureManager::validateState()
{
    if (texturesBuilt_)
    {
        std::cerr << "ERROR: TextureManager::buildAllTextures() already called" << std::endl;
        throw std::logic_error("Duplicate function call");
    }

    for (AssetLoader::DeferredTexture &texture : deferredTextures_)
    {
        auto iter = materialLookup_.find(texture.handle);

        if (iter == materialLookup_.end())
            missingHandleError(texture.handle);
    }
}

void TextureManager::addMaterials(Handle handle, std::size_t size)
{
    auto rangeIter = materialLookup_.find(handle);

    if (rangeIter == materialLookup_.end())
        missingHandleError(handle);

    auto &[_, range] = *rangeIter;

    std::size_t begin{materials_.size()};
    std::size_t end{begin + size};

    range.begin = begin;
    range.end = end;

    materials_.resize(materials_.size() + size);
}

std::span<Material> TextureManager::findMaterials(Handle handle)
{
    auto iter = materialLookup_.find(handle);

    if (iter == materialLookup_.end())
        missingHandleError(handle);

    MaterialRange range{iter->second};

    return std::span<Material>{materials_}.subspan(range.begin, range.end - range.begin);
}

void TextureManager::assignFallbacks()
{
    if (materials_.empty())
        return;

    SamplerArrayIndex fallbackTexture;

    for (int i{0}; i < static_cast<int>(arrayTextures_.size()); ++i)
    {
        if (arrayTextures_[i].getFallbackLayer().has_value())
        {
            fallbackTexture.array = i;
            fallbackTexture.layer = arrayTextures_[i].getFallbackLayer().value();
            break;
        }
    }

    SamplerArrayIndex fallbackCubemap;

    for (int i{0}; i < static_cast<int>(arrayCubemaps_.size()); ++i)
    {
        if (arrayCubemaps_[i].getFallbackLayer().has_value())
        {
            fallbackCubemap.array = i;
            fallbackCubemap.layer = arrayCubemaps_[i].getFallbackLayer().value();
            break;
        }
    }

    bool needCubemap{false};
    bool needTextures{false};

    std::vector<Material::Type> pbr = {Material::albedo, Material::ARM, Material::normal};

    for (Material &material : materials_)
    {
        for (Material::Type t : pbr)
        {
            if (material[t].array == -1 || material[t].layer == -1)
            {
                material[t] = fallbackTexture;
                needTextures = true;
            }
        }

        if (material[Material::cubemap].array == -1 || material[Material::cubemap].layer == -1)
        {
            material[Material::cubemap] = fallbackCubemap;
            needCubemap = true;
        }
    }

    if (needTextures && fallbackTexture.array == -1)
    {
        std::cerr << "ERROR: Fallback texture not found" << std::endl;
        throw std::runtime_error("Fallback cubemap not found");
    }

    if (needCubemap && fallbackCubemap.array == -1)
    {
        std::cerr << "ERROR: Fallback cubemap not found" << std::endl;
        throw std::runtime_error("Fallback cubemap not found");
    }
}
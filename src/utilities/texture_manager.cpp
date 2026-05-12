#include "texture_manager.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "array_texture.h"
#include "asset_loader.h"
#include "fastgltf/types.hpp"
#include "glad/glad.h"
#include "gli/texture.hpp"
#include "graphics_settings.h"
#include "handle.h"
#include "render_target.h"
#include "texture_loader.h"
#include "texture_utilities.h"
#include "textures.h"
#include "thread_pool.h"

TextureManager::TextureManager(ThreadPool &threadPool, const GraphicsSettings &settings, int reflectionResolution)
    : threadPool_{threadPool}, globalAnisotropicDegree_{static_cast<float>(settings.anisotropicFiltering)}
{

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

    arrayTextures_.emplace_back(ArrayTexture{512, 512, GL_COMPRESSED_RG_RGTC2});

    arrayCubemaps_.reserve(3);

    arrayCubemaps_.emplace_back(ArrayTexture{4096, 4096, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM});
    arrayCubemaps_.emplace_back(ArrayTexture{2048, 2048, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM});
    arrayCubemaps_.emplace_back(
        ArrayTexture{reflectionResolution, reflectionResolution, dynamicCubemapInternalFormat_});
}

std::vector<Material> &TextureManager::findMaterials(Handle handle)
{
    return const_cast<std::vector<Material> &>(std::as_const(*this).getMaterials(handle));
}

const std::vector<Material> &TextureManager::getMaterials(Handle handle) const
{
    auto iter = materialLookup_.find(handle);

    if (iter == materialLookup_.end())
    {
        std::cerr << "ERROR: missing handle for " << handle << " .Could not get materials " << std::endl;
        throw std::runtime_error("Missing handle");
    }
    return iter->second;
}

// Returns the internal color format used for dynamic cubemaps
GLenum TextureManager::getReflectionColorFormat() const
{
    return dynamicCubemapInternalFormat_;
}

Handle TextureManager::assignHandle()
{
    static Handle nextHandle{1};

    Handle handle{nextHandle++};

    materialLookup_[handle];

    return handle;
}

void TextureManager::loadModelTextures(const Handle handle, fastgltf::Asset &&asset,
                                       const std::filesystem::path &parentPath,
                                       const std::optional<Handle> borrowHandle)
{
    auto &materials = findMaterials(handle);
    materials.resize(asset.materials.size());

    if (borrowHandle.has_value())
    {
        pendingBorrows_.emplace_back(std::pair{handle, borrowHandle.value()});
        return;
    }

    const std::vector<AssetLoader::DeferredTexture> loaded{AssetLoader::texturesFromAsset(asset, parentPath, handle)};
    deferredTextures_.insert(deferredTextures_.end(), loaded.begin(), loaded.end());
}

void TextureManager::loadCubemapTextures(const Handle handle, const std::filesystem::path &cubePath,
                                         std::vector<int> &materialIndices)
{
    // This needs to deal with borrowHandle too

    if (cubePath.empty() || !std::filesystem::exists(cubePath))
        return;

    auto &materials = findMaterials(handle);

    // Compensate for models that want a cubemap but have no materials
    if (materials.empty())
        materials.resize(1);

    deferredTextures_.emplace_back(AssetLoader::DeferredTexture{
        .handle = handle,
        .path = cubePath,
        .textureRepeat = false,
        .materialIndices = materialIndices,
        .type = Material::cubemap,
    });
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

    std::vector<gli::texture> textures{loadTexturesFromPool(deferredTextures_)};

    for (std::size_t i{0}; i < deferredTextures_.size(); ++i)
        countTexture(deferredTextures_[i], textures[i]);

    for (ArrayTexture &array : arrayTextures_)
        array.allocate(false, true, true, globalAnisotropicDegree_);

    for (ArrayTexture &array : arrayCubemaps_)
        array.allocate(true, false, true, globalAnisotropicDegree_);

    for (std::size_t i{0}; i < deferredTextures_.size(); ++i)
        insertTexture(deferredTextures_[i], textures[i]);

    for (int i{0}; i < static_cast<int>(arrayTextures_.size()); ++i)
        arrayTextures_[i].bindTextureUnit(TextureUnit::array2DBase + i);

    for (int i{0}; i < static_cast<int>(arrayCubemaps_.size()); ++i)
        arrayCubemaps_[i].bindTextureUnit(TextureUnit::arrayCubemapBase + i);

    resolveBorrows();
}

void TextureManager::resolveBorrows()
{
    for (auto [self, other] : pendingBorrows_)
    {
        auto &selfMaterials{findMaterials(self)};
        auto &otherMaterials{findMaterials(other)};

        if (selfMaterials.size() != otherMaterials.size())
            std::cerr << "WARNING: " << self << " borrowing " << selfMaterials.size() << " from " << other << " with "
                      << otherMaterials.size() << " - different material count" << std::endl;

        selfMaterials = otherMaterials;
    }
}

void TextureManager::reserveCubemaps(int numCubemaps, int resolution, GLenum internalFormat)
{
    if (texturesBuilt_)
    {
        std::cerr << "ERROR: Cannot reserve after TextureManager::buildAllTextures() called" << std::endl;
        throw std::logic_error("Late function call");
    }

    ArrayReference arrayRef{findCubemapArray(resolution, internalFormat)};
    arrayRef.array.addCapacity(numCubemaps);
}

// Overwrites any existing cubemaps for this model
void TextureManager::setCubemap(Handle handle, std::vector<int> overrideIndices, RenderTarget &renderTarget)
{
    const int resolution{renderTarget.getWidth()};
    ArrayReference arrayRef{findCubemapArray(resolution, dynamicCubemapInternalFormat_)};

    int layer{arrayRef.array.insert(renderTarget)};
    SamplerArrayIndex arrayIndex{.array = arrayRef.index, .layer = layer};

    arrayRef.array.generateMipmaps();

    auto &materials = findMaterials(handle);

    for (int materialIndex : overrideIndices)
    {
        if (materialIndex >= static_cast<int>(materials.size()))
            continue;

        Material &material{materials[materialIndex]};
        SamplerArrayIndex &entry{material.layers[Material::cubemap]};

        if (entry.array != -1 || entry.layer != -1)
            std::cerr << "Warning: Orphaning own cubemaps for handle " << handle << std::endl;

        entry = arrayIndex;
    }
}

void TextureManager::borrowCubemaps(Handle self, Handle other)
{
    if (!texturesBuilt_)
    {
        std::cerr << "ERROR: Cannot borrow before TextureManager::buildAllTextures() called" << std::endl;
        throw std::logic_error("Early function call");
    }

    auto &selfMaterials{findMaterials(self)};
    auto &otherMaterials{findMaterials(other)};

    if (selfMaterials.size() != otherMaterials.size())
        std::cerr << "WARNING: " << self << " borrowing mismatched material size from " << other << std::endl;

    for (std::size_t i{0}; i < selfMaterials.size() && i < otherMaterials.size(); ++i)
    {
        SamplerArrayIndex &selfArray{selfMaterials[i].layers[Material::cubemap]};
        SamplerArrayIndex &otherArray{otherMaterials[i].layers[Material::cubemap]};

        if (selfArray.array != -1 || selfArray.layer != -1)
            std::cerr << "Warning: Orphaning cubemap for handle " << self << ", borrowing from " << other << std::endl;

        selfArray = otherArray;
    }
}

std::vector<gli::texture> TextureManager::loadTexturesFromPool(
    const std::vector<AssetLoader::DeferredTexture> &textures)
{
    std::vector<std::filesystem::path> paths;

    paths.reserve(textures.size());

    for (const AssetLoader::DeferredTexture &texture : textures)
        paths.emplace_back(texture.path);

    TextureLoader loader{threadPool_, paths};

    return loader.getTextures();
}

void TextureManager::insertTexture(const AssetLoader::DeferredTexture &deferred, const gli::texture &texture)
{
    ArrayReference arrayRef{findArray(deferred, texture)};
    int layer{arrayRef.array.insert(texture)};
    SamplerArrayIndex arrayIndex{.array = arrayRef.index, .layer = layer};

    std::vector<Material> &materials{materialLookup_.at(deferred.handle)};

    for (int idx : deferred.materialIndices)
    {
        Material &material{materials[idx]};
        material.layers[deferred.type] = arrayIndex;
    }
}

TextureManager::ArrayReference TextureManager::findArray(const AssetLoader::DeferredTexture &deferred,
                                                         const gli::texture &texture)
{
    const bool isCubemap{deferred.type == Material::cubemap};
    std::vector<ArrayTexture> &arrayTextures{isCubemap ? arrayCubemaps_ : arrayTextures_};

    glm::tvec3<GLsizei> extent{texture.extent()};
    int width{static_cast<int>(extent.x)};
    int height{static_cast<int>(extent.y)};

    GLenum internalFormat{TextureUtilities::gliToGL(texture)};

    // Slightly annoying. Nvidia Texture Tools does not tag anything as sRGB
    if (isCubemap || deferred.type == Material::albedo)
        internalFormat = TextureUtilities::toSRGB(internalFormat);

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

void TextureManager::countTexture(const AssetLoader::DeferredTexture &deferred, const gli::texture &texture)
{
    ArrayReference arrayRef{findArray(deferred, texture)};
    arrayRef.array.addCapacity(1);
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
        {
            std::cerr << "ERROR: missing handle for " << texture.handle << " .Could not get materials " << std::endl;
            throw std::runtime_error("Missing handle");
        }
    }
}
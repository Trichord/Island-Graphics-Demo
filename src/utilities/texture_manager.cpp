#include "texture_manager.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "fastgltf/types.hpp"
#include "fastgltf_loader.h"
#include "glad/glad.h"
#include "gli/gli.hpp"
#include "graphics_settings.h"
#include "render_target.h"
#include "texture_loader.h"
#include "texture_utilities.h"
#include "textures.h"
#include "thread_pool.h"

TextureManager::TextureManager(ThreadPool &threadPool, const GraphicsSettings &settings, int reflectionResolution)
    : threadPool_{threadPool}, globalAnisotropicDegree_{static_cast<float>(settings.anisotropicFiltering)}
{
    cubemapArrayTextures_.back() = {
        .size = reflectionResolution,
        .internalFormat = dynamicCubemapInternalFormat_,
    };
}

TextureManager::~TextureManager()
{
    for (ArrayTexture &texture : arrayTextures_)
        if (texture.id != 0)
            glDeleteTextures(1, &texture.id);

    for (ArrayTexture &texture : cubemapArrayTextures_)
        if (texture.id != 0)
            glDeleteTextures(1, &texture.id);
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

    texturesFromAsset(asset, handle, parentPath);
}

void TextureManager::loadCubemapTextures(const Handle handle, const std::filesystem::path &cubePath,
                                         std::vector<int> &materialIndices)
{
    // This needs a borrowHandle constructor too

    if (cubePath.empty() || !std::filesystem::exists(cubePath))
        return;

    auto &materials = findMaterials(handle);

    // Compensate for models that want a cubemap but have no materials
    if (materials.empty())
        materials.resize(1);

    deferredCubemaps_.emplace_back(DeferredTexture{
        .handle = handle,
        .path = cubePath,
        .type = Material::cubemap,
        .textureRepeat = false,
        .materialIndices = materialIndices,
    });
}

void TextureManager::buildAllTextures()
{
    if (texturesBuilt_)
    {
        std::cerr << "ERROR: TextureManager::buildAllTextures() already called" << std::endl;
        throw std::logic_error("Duplicate function call");
    }

    allocateAndFillArray(arrayTextures_, deferredTextures_, false);
    allocateAndFillArray(cubemapArrayTextures_, deferredCubemaps_, true);

    texturesBuilt_ = true;
}

void TextureManager::allocateAndFillArray(auto &array, std::vector<DeferredTexture> &deferred, bool cubemaps)
{
    if (deferred.empty())
        return;

    std::vector<gli::texture> textures{loadTexturesFromPool(deferred)};

    for (std::size_t i{0}; i < deferred.size(); ++i)
        if (!validateTexture(textures[i]))
        {
            std::cout << deferred[i].path << std::endl;
            throw std::runtime_error("Failed to load texture");
        }

    // Update the size and internal type and count, now that we actually have that information
    for (std::size_t i{0}; i < deferred.size(); ++i)
    {
        // This is really annoying. Nvidia Texture Tools doesn't export anything as SRGB, so it has to be promoted
        promoteToSRGB(deferred[i], textures[i]);

        const glm::tvec3<GLsizei> extent{textures[i].extent()};
        deferred[i].size = static_cast<int>(extent.x);
        ArrayTexture &arrayEntry = findArrayEntry(array, deferred[i].size, deferred[i].internalFormat);
        ++arrayEntry.count;
    }

    createTextureLayers(array, cubemaps);

    for (std::size_t i{0}; i < textures.size(); ++i)
        insertTexture(array, deferred[i], textures[i]);

    if (!cubemaps)
        resolveBorrows(); // add cubemaps here too
}

// Queues up pending textures for an asset, to be inserted into the texture array later
void TextureManager::texturesFromAsset(fastgltf::Asset &asset, Handle handle, std::filesystem::path parentPath)
{
    // Resize to match fastgltf materials array
    auto &materials = findMaterials(handle);
    materials.resize(asset.materials.size());

    // Get image paths image data
    const std::vector<std::filesystem::path> paths = fastgltfLoader::getURIs(asset, parentPath);

    // Map image index to pending texture index for deduplication
    std::unordered_map<std::size_t, std::size_t> imageIndices;

    // If the image index already exists, add the material index
    auto getOrCreateTextureID = [&](const fastgltf::TextureInfo &textureInfo, Material::Type type,
                                    std::size_t materialIndex) {
        TextureDescriptor descriptor{fastgltfLoader::getTextureInfo(asset, textureInfo)};
        auto [pendingTextureIndex, inserted] = imageIndices.try_emplace(descriptor.imageIndex, 0);

        if (descriptor.sampler.wrapS != descriptor.sampler.wrapT)
        {
            std::cerr << "Sampler wrap mismatch";
            throw std::runtime_error("Texture property error");
        }

        if (inserted)
        {
            pendingTextureIndex->second = deferredTextures_.size();
            const bool repeat{descriptor.sampler.wrapS == static_cast<uint16_t>(fastgltf::Wrap::Repeat)};

            deferredTextures_.emplace_back(DeferredTexture{
                .handle = handle,
                .path = {paths[descriptor.imageIndex]},
                .type = type,
                .textureRepeat = repeat,
                .materialIndices = {static_cast<int>(materialIndex)},
            });
        }
        else
        {
            deferredTextures_[pendingTextureIndex->second].materialIndices.push_back(static_cast<int>(materialIndex));
        }
    };

    for (size_t i{0}; i < asset.materials.size(); ++i)
    {
        const fastgltf::Material &assetMaterial = asset.materials[i];
        Material material;

        if (assetMaterial.pbrData.baseColorTexture.has_value())
        {
            const fastgltf::TextureInfo &albedoInfo{assetMaterial.pbrData.baseColorTexture.value()};
            getOrCreateTextureID(albedoInfo, Material::albedo, i);
        }

        if (assetMaterial.pbrData.metallicRoughnessTexture.has_value())
        {
            // ARM map never gets sRGB or anisotropic filtering
            const fastgltf::TextureInfo &armInfo = assetMaterial.pbrData.metallicRoughnessTexture.value();
            getOrCreateTextureID(armInfo, Material::ARM, i);
        }

        if (assetMaterial.normalTexture.has_value())
        {
            // Normal maps get no sRGB or aniso
            const fastgltf::TextureInfo &normalInfo = assetMaterial.normalTexture.value();
            getOrCreateTextureID(normalInfo, Material::normal, i);
        }
    }
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

    ArrayTexture &arrayTexture{findArrayEntry(cubemapArrayTextures_, resolution, internalFormat)};
    arrayTexture.count += numCubemaps;
}

// Overwrites any existing cubemaps for this model
void TextureManager::setCubemap(Handle handle, std::vector<int> overrideIndices, RenderTarget &renderTarget)
{
    const int size{renderTarget.getWidth()};

    SamplerArrayEntry reservedEntry{assignToTextureArray(cubemapArrayTextures_, size, dynamicCubemapInternalFormat_)};
    ArrayTexture &arrayTexture{cubemapArrayTextures_[reservedEntry.array]};

    if (arrayTexture.id == 0)
        return;

    GLuint sourceTextureID{renderTarget.getColorTextureID()};

    glCopyImageSubData(sourceTextureID, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, arrayTexture.id, GL_TEXTURE_CUBE_MAP_ARRAY, 0,
                       0, 0, static_cast<GLint>(reservedEntry.layer * 6), size, size, 6);

    glGenerateTextureMipmap(arrayTexture.id);

    auto &materials = findMaterials(handle);

    for (int materialIndex : overrideIndices)
    {
        if (materialIndex >= static_cast<int>(materials.size()))
            continue;

        Material &material{materials[materialIndex]};
        SamplerArrayEntry &entry{material.layers[Material::cubemap]};

        if (entry.array != -1 || entry.layer != -1)
            std::cerr << "Warning: Orphaning own cubemaps for handle " << handle << std::endl;

        entry = reservedEntry;
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
        SamplerArrayEntry &selfArray{selfMaterials[i].layers[Material::cubemap]};
        SamplerArrayEntry &otherArray{otherMaterials[i].layers[Material::cubemap]};

        if (selfArray.array != -1 || selfArray.layer != -1)
            std::cerr << "Warning: Orphaning cubemap for handle " << self << ", borrowing from " << other << std::endl;

        selfArray = otherArray;
    }
}

std::vector<gli::texture> TextureManager::loadTexturesFromPool(const std::vector<DeferredTexture> &textures)
{
    std::vector<std::filesystem::path> paths;

    paths.reserve(textures.size());

    for (const DeferredTexture &texture : textures)
        paths.emplace_back(texture.path);

    TextureLoader loader{threadPool_, paths};

    return loader.getTextures();
}

void TextureManager::promoteToSRGB(DeferredTexture &deferred, const gli::texture &texture)
{
    GLenum internalFormat{textureUtilities::gliToGL(texture)};
    const bool sRGB{deferred.type == Material::Type::albedo || deferred.type == Material::Type::cubemap};

    if (sRGB)
        deferred.internalFormat = textureUtilities::toSRGB(internalFormat);
    else
        deferred.internalFormat = internalFormat;
}

void TextureManager::insertTexture(auto &array, const DeferredTexture &deferred, const gli::texture &texture)
{
    SamplerArrayEntry assignedEntry{assignToTextureArray(array, deferred.size, deferred.internalFormat)};
    ArrayTexture& arrayTexture{array[assignedEntry.array]};

    if (arrayTexture.id == 0)
    {
        std::cerr << "ERROR: Uninitialized array texture size " << arrayTexture.size << " format "
                  << arrayTexture.internalFormat
                  << std::endl;

        throw std::runtime_error("Uninitialized array texture");
    }

    const Handle handle{deferred.handle};
    std::vector<Material> &materials{materialLookup_.at(handle)};

    for (auto index : deferred.materialIndices)
        materials[index].layers[deferred.type] = assignedEntry;

    textureUtilities::insertIntoSamplerArray(texture, arrayTexture.id, assignedEntry.layer,
                                             deferred.internalFormat);
}

void TextureManager::createTextureLayers(auto &arrayTextures, bool cubemaps)
{
    for (std::size_t i{0}; i < arrayTextures.size(); ++i)
    {
        if (!arrayTextures[i].count)
            continue;

        ArrayTexture &arrayTexture{arrayTextures[i]};
        GLuint &arrayTexID{arrayTexture.id};
        GLsizei numTextures{static_cast<GLsizei>(arrayTexture.count)};

        if (cubemaps)
            numTextures *= 6;

        int size{arrayTexture.size};
        int mipLevels{textureUtilities::calculateMipmapLevels(size, size)};

        if (cubemaps)
            glCreateTextures(GL_TEXTURE_CUBE_MAP_ARRAY, 1, &arrayTexID);
        else
            glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &arrayTexID);

        glTextureStorage3D(arrayTexID, mipLevels, arrayTexture.internalFormat, size, size, numTextures);

        if (cubemaps)
        {
            glTextureParameteri(arrayTexID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(arrayTexID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTextureParameteri(arrayTexID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        }
        else
        {
            glTextureParameteri(arrayTexID, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTextureParameteri(arrayTexID, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }

        glTextureParameteri(arrayTexID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(arrayTexID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (globalAnisotropicDegree_ > 0.0f)
            glTextureParameterf(arrayTexID, GL_TEXTURE_MAX_ANISOTROPY, globalAnisotropicDegree_);

        GLuint textureUnitOffset{cubemaps ? static_cast<GLuint>(TextureUnit::cubemapSRGB4k)
                                          : static_cast<GLuint>(TextureUnit::RG4k)};

        glBindTextureUnit(static_cast<GLuint>(textureUnitOffset + i), arrayTexID);
    }
}

TextureManager::ArrayTexture &TextureManager::findArrayEntry(auto &arrayTextures, int size, GLenum internalFormat)
{
    for (auto &texture : arrayTextures)
        if (size == texture.size && internalFormat == texture.internalFormat)
            return texture;

    std::cerr << "No matching array for " << size << " format " << internalFormat << std::endl;
    throw std::runtime_error("ArrayTexture bucket not found");
}

// Fills a texture array back to front, using the existing ArrayTexture's count as a tracker of how many slots are left
SamplerArrayEntry TextureManager::assignToTextureArray(auto &arrayTextures, int size, GLenum internalFormat)
{
    for (std::size_t i{0}; i < arrayTextures.size(); ++i)
    {
        if (size == arrayTextures[i].size && internalFormat == arrayTextures[i].internalFormat)
        {
            if (arrayTextures[i].count < 0)
            {
                std::cerr << "ERROR: Array texture " << size << " overflow" << std::endl;
                throw std::runtime_error("Array texture overflow");
            }

            return {static_cast<int>(i), --arrayTextures[i].count};
        }
    }

    std::cerr << "No matching array for " << size << " format " << internalFormat << std::endl;
    throw std::runtime_error("ArrayTexture bucket not found");
}

bool TextureManager::validateTexture(const gli::texture &texture)
{
    const glm::tvec3<GLsizei> extent{texture.extent()};

    bool valid{true};

    if (texture.empty())
    {
        std::cerr << "ERROR: Empty texture" << std::endl;
        valid = false;
    }

    else if (extent.x != extent.y)
    {
        std::cerr << "ERROR: non-square texture" << std::endl;
        valid = false;
    }

    else if (!gli::is_compressed(texture.format()))
    {
        std::cerr << "ERROR: uncompressed texture" << std::endl;
        valid = false;
    }

    return valid;
}
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

namespace
{

std::pair<int, gli::gl::internal_format> gliToGL(const gli::gl &translator, gli::texture &texture)
{
    const gli::gl::format format = translator.translate(texture.format(), texture.swizzles());

    const glm::tvec3<GLsizei> extent{texture.extent()};
    const int size{static_cast<int>(extent.x)};

    return {size, format.Internal};
}

void assignFallbackTexture(Material &material, Material::Type type)
{
    SamplerArrayEntry fallback{.array = 0, .layer = 0};
    material.layers[type] = fallback;
}

} // namespace

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
    //This needs a borrowHandle constructor too

    if (cubePath.empty() || !std::filesystem::exists(cubePath))
        return;

    auto &materials = findMaterials(handle);

    // Compensate for models that want a cubemap but have no materials
    if (materials.empty())
        materials.resize(1);

    pendingCubemaps_.emplace_back(PendingTexture{
        .handle = handle,
        .path = cubePath,
        .type = Material::cubemap,
        .textureRepeat = false,
        .materialIndices = materialIndices,
    });
}

std::vector<Material> &TextureManager::findMaterials(Handle handle)
{
    auto iter = materialLookup_.find(handle);

    if (iter == materialLookup_.end())
    {
        std::cerr << "ERROR: missing handle for " << handle << " .Could not get materials " << std::endl;
        throw std::runtime_error("Missing handle");
    }
    return iter->second;
}

const std::vector<Material> &TextureManager::getMaterials(Handle handle) const
{
    return const_cast<TextureManager *>(this)->findMaterials(handle);
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

void TextureManager::buildAllTextures()
{
    if (texturesBuilt_)
    {
        std::cerr << "ERROR: TextureManager::buildAllTextures() already called" << std::endl;
        throw std::logic_error("Duplicate function call");
    }

    std::cout << "Loading 2D Textures... ";
    buildTextures(pending2dTextures_, false);
    std::cout << "Done." << std::endl;

    std::cout << "Loading cubemaps... ";
    buildTextures(pendingCubemaps_, true);
    std::cout << "Done." << std::endl;

    resolveBorrows();

    texturesBuilt_ = true;
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
            pendingTextureIndex->second = pending2dTextures_.size();
            const bool repeat{descriptor.sampler.wrapS == static_cast<uint16_t>(fastgltf::Wrap::Repeat)};

            pending2dTextures_.emplace_back(PendingTexture{
                .handle = handle,
                .path = {paths[descriptor.imageIndex]},
                .type = type,
                .textureRepeat = repeat,
                .materialIndices = {static_cast<int>(materialIndex)},
            });
        }
        else
        {
            pending2dTextures_[pendingTextureIndex->second].materialIndices.push_back(static_cast<int>(materialIndex));
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
    // if buildCubemaps() already called, throw

    for (std::size_t i{0}; i < cubemapArrayTextures_.size(); ++i)
    {
        if (cubemapArrayTextures_[i].size == resolution && cubemapArrayTextures_[i].internalFormat == internalFormat)
        {
            cubemapArrayTextures_[i].count += numCubemaps;
            return;
        }
    }

    std::cerr << "ERROR: Cubemap reservation failure. No matching cubemap array for " << resolution << ", format "
              << std::to_string(internalFormat) << std::endl;
    throw std::runtime_error("ArrayTexture bucket not found");
}

// Overwrites any existing cubemaps for this model
void TextureManager::setCubemap(Handle handle, std::vector<int> overrideIndices, RenderTarget &renderTarget)
{
    const int size{renderTarget.getWidth()};

    SamplerArrayEntry reservedEntry{assignToCubeArray(size, dynamicCubemapInternalFormat_)};
    ArrayTexture &arrayTexture{cubemapArrayTextures_[reservedEntry.array]};

    if (arrayTexture.id == 0)
        return;

    GLuint sourceTextureID{renderTarget.getColorTextureID()};

    glCopyImageSubData(sourceTextureID, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, arrayTexture.id, GL_TEXTURE_CUBE_MAP_ARRAY, 0,
                       0, 0, static_cast<GLint>(reservedEntry.layer * 6), size, size, 6);

    glGenerateTextureMipmap(arrayTexture.id);


    auto& materials = findMaterials(handle);

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
    // if cubemaps not loaded, throw logic error

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

std::vector<gli::texture> TextureManager::loadTexturesFromPool(const std::vector<PendingTexture> &textures)
{
    std::vector<std::filesystem::path> paths;

    paths.reserve(textures.size());

    for (const PendingTexture &texture : textures)
        paths.emplace_back(texture.path);

    TextureLoader loader{threadPool_, paths};

    return loader.getTextures();
}

void TextureManager::buildTextures(std::vector<PendingTexture> &pendingTextures, bool cubemaps)
{
    if (pendingTextures.empty())
        return;

    std::vector<gli::texture> textures{loadTexturesFromPool(pendingTextures)};

    if (cubemaps)
        allocateCubemaps(pendingTextures, textures);
    else
        allocateTextures(pendingTextures, textures);

    const gli::gl glTranslator(gli::gl::PROFILE_GL33);

    for (std::size_t i{0}; i < pendingTextures.size(); ++i)
    {
        PendingTexture &pendingTexture{pendingTextures[i]};
        gli::texture &texture{textures[i]};

        const Handle handle{pendingTexture.handle};
        std::vector<Material> &materials{materialLookup_.at(handle)};

        if (texture.empty())
        {
            std::cerr << "ERROR: Missing texture for " << pendingTexture.path << std::endl;

            // Because of gLTF deduplicaion, multiple materials might point to the same texture
            for (auto index : pendingTexture.materialIndices)
                assignFallbackTexture(materials[index], pendingTexture.type);

            continue;
        }

        const glm::tvec3<GLsizei> extent{texture.extent()};

        if (extent.x != extent.y)
        {
            std::cerr << "ERROR: non-square texture for " << pendingTexture.path << std::endl;

            for (auto index : pendingTexture.materialIndices)
                assignFallbackTexture(materials[index], pendingTexture.type);

            continue;
        }

        if (!gli::is_compressed(texture.format()))
        {
            std::cerr << "ERROR: uncompressed texture for " << pendingTexture.path << std::endl;

            for (auto index : pendingTexture.materialIndices)
                assignFallbackTexture(materials[index], pendingTexture.type);

            continue;
        }

        auto [size, format] = gliToGL(glTranslator, texture);

        // This is really annoying. Nvidia Texture Tools doesn't export anything as SRGB, so it has to be promoted
        const bool sRGB{pendingTexture.type == Material::Type::albedo ||
                        pendingTexture.type == Material::Type::cubemap};
        if (sRGB)
            format = textureUtilities::toSRGB(format);

        SamplerArrayEntry assignedArray{cubemaps ? assignToCubeArray(size, static_cast<GLenum>(format))
                                                 : assignToTextureArray(size, static_cast<GLenum>(format))};

        for (auto index : pendingTexture.materialIndices)
            materials[index].layers[pendingTexture.type] = assignedArray;

        ArrayTexture &arrayTexture{cubemaps ? cubemapArrayTextures_[assignedArray.array]
                                            : arrayTextures_[assignedArray.array]};

        if (arrayTexture.id == 0)
        {
            std::cerr << "ERROR: Uninitialized array texture size " << arrayTexture.size << " format "
                      << arrayTexture.internalFormat << std::endl;

            throw std::runtime_error("Uninitialized array texture");
        }

        textureUtilities::insertIntoSamplerArray(arrayTexture.id, texture, assignedArray.layer, format);
    }
}

void TextureManager::allocateTextures(const std::vector<PendingTexture> &pendingTextures,
                                      std::vector<gli::texture> &textures)
{
    auto addToBucket = [&](int size, GLenum format) {
        for (std::size_t i{0}; i < arrayTextures_.size(); ++i)
            if (size == arrayTextures_[i].size && format == arrayTextures_[i].internalFormat)
                ++arrayTextures_[i].count;
    };

    // 4.6 unavailable, texture enums are the same between 3.3 and 4.6
    const gli::gl glTranslator(gli::gl::PROFILE_GL33);

    for (std::size_t i{0}; i < textures.size(); ++i)
    {
        if (textures[i].empty())
            continue;

        auto [size, format] = gliToGL(glTranslator, textures[i]);

        const bool sRGB{pendingTextures[i].type == Material::Type::albedo ||
                        pendingTextures[i].type == Material::Type::cubemap};
        if (sRGB)
            format = textureUtilities::toSRGB(format);

        addToBucket(size, static_cast<GLenum>(format));
    }

    for (std::size_t i{0}; i < arrayTextures_.size(); ++i)
    {
        if (!arrayTextures_[i].count)
            continue;

        ArrayTexture &arrayTexture{arrayTextures_[i]};
        GLuint &arrayTexID{arrayTexture.id};
        GLsizei numTextures{static_cast<GLsizei>(arrayTexture.count)};

        int size{arrayTexture.size};
        int mipLevels{textureUtilities::calculateMipmapLevels(size, size)};

        glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &arrayTexID);
        glTextureStorage3D(arrayTexID, mipLevels, arrayTexture.internalFormat, size, size, numTextures);

        glTextureParameteri(arrayTexID, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(arrayTexID, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTextureParameteri(arrayTexID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(arrayTexID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (globalAnisotropicDegree_ > 0.0f)
            glTextureParameterf(arrayTexID, GL_TEXTURE_MAX_ANISOTROPY, globalAnisotropicDegree_);

        glBindTextureUnit(static_cast<GLuint>(i), arrayTexID);
    }
}

void TextureManager::allocateCubemaps(const std::vector<PendingTexture> &pendingTextures,
                                      std::vector<gli::texture> &textures)
{
    auto addToBucket = [&](int size, GLenum format) {
        for (std::size_t i{0}; i < cubemapArrayTextures_.size(); ++i)
            if (size == cubemapArrayTextures_[i].size && format == cubemapArrayTextures_[i].internalFormat)
                ++cubemapArrayTextures_[i].count;
    };

    const gli::gl glTranslator(gli::gl::PROFILE_GL33);

    for (std::size_t i{0}; i < textures.size(); ++i)
    {
        if (textures[i].empty())
            continue;

        auto [size, format] = gliToGL(glTranslator, textures[i]);

        const bool sRGB{pendingTextures[i].type == Material::Type::albedo ||
                        pendingTextures[i].type == Material::Type::cubemap};
        if (sRGB)
            format = textureUtilities::toSRGB(format);

        addToBucket(size, static_cast<GLenum>(format));
    }

    for (std::size_t i{0}; i < cubemapArrayTextures_.size(); ++i)
    {
        if (!cubemapArrayTextures_[i].count)
            continue;

        ArrayTexture &arrayTexture{cubemapArrayTextures_[i]};
        GLuint &arrayTexID{arrayTexture.id};
        GLsizei numTextures{static_cast<GLsizei>(arrayTexture.count)};

        int width{arrayTexture.size};
        int mipLevels{textureUtilities::calculateMipmapLevels(width, width)};

        glCreateTextures(GL_TEXTURE_CUBE_MAP_ARRAY, 1, &arrayTexID);
        glTextureStorage3D(arrayTexID, mipLevels, arrayTexture.internalFormat, width, width, numTextures * 6);

        glTextureParameteri(arrayTexID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(arrayTexID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(arrayTexID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTextureParameteri(arrayTexID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(arrayTexID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (globalAnisotropicDegree_ > 0.0f)
            glTextureParameterf(arrayTexID, GL_TEXTURE_MAX_ANISOTROPY, globalAnisotropicDegree_);

        constexpr GLuint cubemapUnitOffset{static_cast<GLuint>(TextureUnit::cubemapSRGB4k)}; // This is brittle
        glBindTextureUnit(static_cast<GLuint>(cubemapUnitOffset + i), arrayTexID);
    }
}

// Fills a texture array back to front, using the existing ArrayTexture's count as a tracker of how many slots are left
SamplerArrayEntry TextureManager::assignToTextureArray(int size, GLenum internalFormat)
{
    for (std::size_t i{0}; i < arrayTextures_.size(); ++i)
        if (size == arrayTextures_[i].size && internalFormat == arrayTextures_[i].internalFormat)
        {
            if (arrayTextures_[i].count < 0)
            {
                std::cerr << "ERROR: Array texture " << size << " overflow" << std::endl;
                throw std::runtime_error("Array texture overflow");
            }

            return {static_cast<int>(i), --arrayTextures_[i].count};
        }

    std::cerr << "No matching 2d array for " << size << " format " << internalFormat << std::endl;
    throw std::runtime_error("ArrayTexture bucket not found");
}

// Fills cubemap texture array back to front, using the existing ArrayTexture's count as a tracker of how many slots are
// left
SamplerArrayEntry TextureManager::assignToCubeArray(int size, GLenum internalFormat)
{
    for (std::size_t i{0}; i < cubemapArrayTextures_.size(); ++i)
    {
        if (size == cubemapArrayTextures_[i].size && internalFormat == cubemapArrayTextures_[i].internalFormat)
        {

            return {static_cast<int>(i), --cubemapArrayTextures_[i].count};
        }
    }

    std::cerr << "No matching cubemap array for " << size << " format " << internalFormat << std::endl;
    throw std::runtime_error("ArrayTexture bucket not found");
}

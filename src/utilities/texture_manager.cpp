#include "texture_manager.h"

#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "fastgltf/types.hpp"
#include "fastgltf_loader.h"
#include "glad/glad.h"
#include "image_loader.h"
#include "texture_units.h"
#include "texture_utilities.h"
#include "thread_pool.h"

constexpr unsigned int threadCount{6u};

void TextureManager::queueModel(const ModelHandle handle, fastgltf::Asset &&asset,
                                const std::filesystem::path &parentPath)
{
    textureCreationQueue.emplace(std::move(asset), handle, parentPath);
}

void TextureManager::queueCubemap(const ModelHandle handle, std::vector<std::filesystem::path> imagePaths,
                                  const std::filesystem::path &parentPath, bool mipMaps, bool sRGB)
{
    // No anisotropic filtering on cubemaps
    textureCreationQueue.emplace(std::move(imagePaths), handle, parentPath, mipMaps, sRGB, false);
}

const std::vector<Material> &TextureManager::getMaterials(ModelHandle handle) const
{
    auto iter = textureCache.find(handle);

    if (iter == textureCache.end())
        throw std::runtime_error("ERROR: Could not aquire materials, handle does not exist");

    return iter->second.materials;
}

void TextureManager::releaseHandle(ModelHandle handle)
{
    auto iter{textureCache.find(handle)};

    if (iter == textureCache.end())
        throw std::runtime_error("ERROR: Could not release handle, handle does not exist");

    TextureEntry &entry{iter->second};

    entry.refCount--;

    if (entry.refCount == 0)
    {
        for (auto &gpuHandle : entry.glTextureHandles)
            glDeleteTextures(1, &gpuHandle);

        textureCache.erase(iter);
    }
}

ModelHandle TextureManager::assignHandle()
{
    static ModelHandle nextHandle{0};

    ModelHandle handle{nextHandle++};

    textureCache.emplace(handle, TextureEntry{});

    return handle;
}

void TextureManager::processTextureQueue()
{
    if (textureCreationQueue.empty())
        return;

    ThreadPool threadPool{threadCount};

    while (!textureCreationQueue.empty())
    {
        auto &element = textureCreationQueue.front();

        // Fastgltf asset
        if (std::holds_alternative<fastgltf::Asset>(element.source))
            textureFromAsset(threadPool, std::get<fastgltf::Asset>(element.source), element.handle, element.parentPath);

        // Cubemap from vector of paths
        else if (std::holds_alternative<std::vector<std::filesystem::path>>(element.source))
            cubemapFromPaths(threadPool, element.handle, std::get<std::vector<std::filesystem::path>>(element.source),
                             element.mipmaps, element.sRGB);
        else
            throw std::runtime_error("ERROR: Unknown source for textures in texture creation queue");

        textureCreationQueue.pop();
    }
}

void TextureManager::textureFromAsset(ThreadPool &threadPool, fastgltf::Asset &asset, ModelHandle handle,
                                      std::filesystem::path parentPath)
{
    auto cacheIter = textureCache.find(handle);

    if (cacheIter == textureCache.end())
        throw std::runtime_error("ERROR: queue element with invalid handle");

    // Get raw image data
    auto paths = fastgltfLoader::getURIs(asset, parentPath);
    ImageLoader imageLoader{threadPool, paths};
    std::vector<LoadedImage> rawImageData = imageLoader.getImageData();

    // Increase reference count and resize to match fastgltf materials array
    TextureEntry &entry{cacheIter->second};
    ++entry.refCount;
    entry.materials.resize(asset.materials.size());

    // Create map to avoid duplicate images
    std::unordered_map<unsigned int, GLuint> indices;

    // Helper function. If the map contains the OpenGL texture ID already, return that. Otherwise, create a new texture
    auto getOrCreateTextureID = [&](const fastgltf::TextureInfo &textureInfo, bool sRGB, bool anisotropic) {
        auto [imageIndex, sampler] = fastgltfLoader::getTextureInfo(asset, textureInfo);
        auto [seenIndices, inserted] = indices.try_emplace(imageIndex, 0);

        if (inserted)
        {
            TextureDetails details{sRGB, anisotropic, anisoLevel};

            GLuint id{
                textureUtilities::generateTextureID(rawImageData[imageIndex], sampler, details, globalAnisoEnabled)};

            // Record new ID as seen
            seenIndices->second = id;

            // Add the ID to the set of all IDs for this model
            entry.glTextureHandles.insert(seenIndices->second);
        }

        return seenIndices->second;
    };

    auto &materials = entry.materials;

    // Process all materials in the gltf asset
    for (size_t i{0}; i < asset.materials.size(); ++i)
    {
        const auto &material = asset.materials[i];

        if (material.pbrData.baseColorTexture.has_value())
        {
            // Albedo textures always get albdeo. Try to force aniso and let the global setting resolve whether it gets
            // applied
            auto &albedo = material.pbrData.baseColorTexture.value();
            materials[i].albedo = getOrCreateTextureID(albedo, true, true);
        }

        if (material.pbrData.metallicRoughnessTexture.has_value())
        {
            // ARM map never gets sRGB or anisotropic filtering
            auto &metallicRoughnessTexture = material.pbrData.metallicRoughnessTexture.value();
            materials[i].occlusionRoughnessMetallic = getOrCreateTextureID(metallicRoughnessTexture, false, false);
        }

        if (material.normalTexture.has_value())
        {
            // Normal maps get no sRGB or aniso
            auto &normalTexture = material.normalTexture.value();
            materials[i].normal = getOrCreateTextureID(normalTexture, false, false);
        }
    }
}

void TextureManager::cubemapFromPaths(ThreadPool &threadPool, ModelHandle handle,
                                      const std::vector<std::filesystem::path> &imagePaths, bool mipMaps, bool sRGB)
{
    auto iter = textureCache.find(handle);

    if (iter == textureCache.end())
        throw std::runtime_error("ERROR: Could not create cubemap, handle does not exist");

    // Increase ref count and resize to 1, since we only have one material
    ++iter->second.refCount;
    iter->second.materials.resize(1);

    // Raw data
    ImageLoader imageLoader{threadPool, imagePaths};
    std::vector<LoadedImage> loadedImages{imageLoader.getImageData()};

    // Create new id, assign it, and track it for eventual release
    GLuint texID{textureUtilities::generateCubemapTextureID(loadedImages, mipMaps, sRGB)};
    iter->second.materials[0].cubemap = texID;
    iter->second.glTextureHandles.insert(texID);
}

const std::vector<Material> &TextureManager::borrowMaterials(ModelHandle other)
{
    auto textureSetIter = textureCache.find(other);

    if (textureSetIter == textureCache.end())
        throw std::runtime_error("ERROR: Could not borrow materials, handle does not exist");

    ++textureSetIter->second.refCount;
    return textureSetIter->second.materials;
}

void TextureManager::setAnisoLevel(float level)
{
    globalAnisoEnabled = true;
    anisoLevel = level;
}

// Assumes caller is giving up control of their texture ID, overwriting anything in that slot
void TextureManager::assignMaterial(ModelHandle handle, TextureUnit textureUnit, GLuint textureID, int materialIndex)
{
    using enum TextureUnit;

    auto iter = textureCache.find(handle);

    if (iter == textureCache.end())
        throw std::runtime_error("ERROR: Could not assign material, handle does not exist");

    auto &materials{iter->second.materials};
    auto &entry{iter->second};

    if (materialIndex >= materials.size())
        return;

    switch (textureUnit)
    {
    case ALBEDO:
        materials[materialIndex].albedo = textureID;
        entry.glTextureHandles.insert(textureID);
        break;

    case ARM:
        materials[materialIndex].occlusionRoughnessMetallic = textureID;
        entry.glTextureHandles.insert(textureID);
        break;

    case NORMAL:
        materials[materialIndex].normal = textureID;
        entry.glTextureHandles.insert(textureID);
        break;

    case CUBEMAP:
        materials[materialIndex].cubemap = textureID;
        entry.glTextureHandles.insert(textureID);
        break;

    default:
        std::cerr << "ERROR: Material not supported in Texture Manager" << std::endl;
    }
}
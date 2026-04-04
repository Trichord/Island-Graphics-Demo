#include "texture_manager.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "fastgltf/types.hpp"
#include "fastgltf_loader.h"
#include "glad/glad.h"
#include "image_loader.h"
#include "texture_units.h"
#include "texture_utilities.h"
#include "thread_pool.h"

TextureManager::TextureManager(ThreadPool &threadPool, const GraphicsSettings &settings)
    : globalAnisotropicDegree_{static_cast<float>(settings.anisotropicFiltering)}, threadPool_{threadPool}
{
}

void TextureManager::loadModel(const ModelHandle handle, fastgltf::Asset &&asset,
                               const std::filesystem::path &parentPath)
{
    textureFromAsset(asset, handle, parentPath);
}

void TextureManager::loadCubemap(const ModelHandle handle, std::vector<std::filesystem::path> imagePaths,
                                 const std::filesystem::path &parentPath, bool mipmaps, bool sRGB)
{
    cubemapFromPaths(handle, std::move(imagePaths), mipmaps, sRGB);
}

const std::vector<Material> &TextureManager::getMaterials(ModelHandle handle) const
{
    auto iter = textureCache_.find(handle);

    if (iter == textureCache_.end())
        throw std::runtime_error("ERROR: Could not aquire materials, handle does not exist");

    return iter->second.materials;
}

void TextureManager::releaseHandle(ModelHandle handle)
{
    auto iter{textureCache_.find(handle)};

    if (iter == textureCache_.end())
        throw std::runtime_error("ERROR: Could not release handle, handle does not exist");

    ModelResources &entry{iter->second};

    if (entry.lender != -1)
    {
        auto otherIter{textureCache_.find(entry.lender)};

        // Non recursive, one level deep on the lender.
        releaseHandle(entry.lender);
        textureCache_.erase(handle);

        return;
    }

    entry.refCount--;

    if (entry.refCount == 0)
    {
        for (auto &gpuHandle : entry.glTextureHandles)
            glDeleteTextures(1, &gpuHandle);

        textureCache_.erase(iter);
    }
}

ModelHandle TextureManager::assignHandle()
{
    static ModelHandle nextHandle{1};

    ModelHandle handle{nextHandle++};

    textureCache_.emplace(handle, ModelResources{});

    return handle;
}

void TextureManager::textureFromAsset(fastgltf::Asset &asset, ModelHandle handle, std::filesystem::path parentPath)
{
    auto cacheIter = textureCache_.find(handle);

    if (cacheIter == textureCache_.end())
        throw std::runtime_error("ERROR: queue element with invalid handle");

    // Get raw image data
    auto paths = fastgltfLoader::getURIs(asset, parentPath);
    ImageLoader imageLoader{threadPool_, paths};
    std::vector<LoadedImage> rawImageData = imageLoader.getImageData();

    // Resize to match fastgltf materials array
    ModelResources &entry{cacheIter->second};
    entry.materials.resize(asset.materials.size());

    // Create map to avoid duplicate images
    std::unordered_map<unsigned int, GLuint> indices;

    // Helper function. If the map contains the OpenGL texture ID already, return that. Otherwise, create a new texture
    auto getOrCreateTextureID = [&](const fastgltf::TextureInfo &textureInfo, bool sRGB, bool anisotropic) {
        auto [imageIndex, sampler] = fastgltfLoader::getTextureInfo(asset, textureInfo);
        auto [seenIndices, inserted] = indices.try_emplace(imageIndex, 0);

        if (inserted)
        {
            // Assume gltf models always want mipmaps
            TextureDetails details{sRGB, true, anisotropic ? globalAnisotropicDegree_ : 0.0f};

            GLuint id{textureUtilities::generateTextureID(rawImageData[imageIndex], sampler, details)};

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
            // Albedo textures always get sRGB. Try to force aniso and let the global setting resolve whether it gets
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

void TextureManager::cubemapFromPaths(ModelHandle handle, const std::vector<std::filesystem::path> &imagePaths,
                                      bool mipmaps, bool sRGB)
{
    auto iter = textureCache_.find(handle);

    if (iter == textureCache_.end())
        throw std::runtime_error("ERROR: Could not create cubemap, handle does not exist");

    // Assume cubemaps never want anisotropic filtering
    TextureDetails textureDetails{sRGB, mipmaps, 0.0f};

    // Resize to 1, since we only have one material
    iter->second.materials.resize(1);

    // Raw data
    ImageLoader imageLoader{threadPool_, imagePaths};
    std::vector<LoadedImage> loadedImages{imageLoader.getImageData()};

    // Create new id, assign it, and track it for eventual release
    GLuint texID{textureUtilities::generateCubemapTextureID(loadedImages, textureDetails)};
    iter->second.materials[0].cubemap = texID;
    iter->second.glTextureHandles.insert(texID);
}

void TextureManager::setMaterial(ModelResources &self, ModelResources &other, Texture texture, int index)
{
    using enum Texture;

    if (index >= self.materials.size() || index >= other.materials.size())
        throw std::runtime_error("ERROR: Material index out of bounds");

    switch (texture)
    {
    case ALBEDO:
        self.materials[index].albedo = other.materials[index].albedo;
        break;
    case ARM:
        self.materials[index].occlusionRoughnessMetallic = other.materials[index].occlusionRoughnessMetallic;
        break;
    case NORMAL:
        self.materials[index].normal = other.materials[index].normal;
        break;
    case CUBEMAP:
        self.materials[index].cubemap = other.materials[index].cubemap;
        break;
    }
}

// Borrow another models materials, completely releasing self's materials
const std::vector<Material> &TextureManager::borrowAssignMaterials(ModelHandle self, ModelHandle other,
                                                                   std::optional<Texture> texture, int materialIndex)
{
    auto selfIter = textureCache_.find(self);
    auto otherIter = textureCache_.find(other);

    if (selfIter == textureCache_.end() || otherIter == textureCache_.end())
        throw std::runtime_error("ERROR: Could not borrow materials, handle does not exist");

    if (otherIter->second.glTextureHandles.empty())
        throw std::runtime_error("ERROR: Could not borrow materials from non-owning handle");

    ModelResources &selfEntry{selfIter->second};
    ModelResources &otherEntry{otherIter->second};

    ++otherEntry.refCount;

    if (texture.has_value())
        setMaterial(selfEntry, otherEntry, texture.value(), materialIndex);
    else
    {
        freeResources(selfEntry);
        selfEntry.materials = otherEntry.materials;
    }

    selfEntry.lender = other;

    return selfIter->second.materials;
}

// Assumes caller is giving up control of their texture ID, overwriting anything in that slot
void TextureManager::setMaterial(ModelHandle handle, Texture texture, GLuint textureID, int materialIndex)
{
    using enum Texture;

    auto iter = textureCache_.find(handle);

    if (iter == textureCache_.end())
        throw std::runtime_error("ERROR: Could not assign material, handle does not exist");

    auto &materials{iter->second.materials};
    auto &entry{iter->second};

    if (materialIndex >= materials.size())
        return;

    switch (texture)
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

void TextureManager::freeResources(ModelResources &resources)
{
    for (auto &gpuHandle : resources.glTextureHandles)
        glDeleteTextures(1, &gpuHandle);

    resources.glTextureHandles.clear();
}
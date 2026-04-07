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
#include "graphics_settings.h"
#include "image_loader.h"
#include "texture_utilities.h"
#include "textures.h"
#include "thread_pool.h"

TextureManager::TextureManager(ThreadPool &threadPool, const GraphicsSettings &settings)
    : globalAnisotropicDegree_{static_cast<float>(settings.anisotropicFiltering)}, threadPool_{threadPool}
{
}

TextureManager::~TextureManager()
{
    for (ArrayTexture &texArr : arrayTextures_)
        for (GLuint id : texArr.textureIDs)
            if (id != 0)
                glDeleteTextures(1, &id);

    for (GLuint id : cubemapIDs_)
        glDeleteTextures(1, &id);
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
    auto iter = materialLookup_.find(handle);

    if (iter == materialLookup_.end())
    {
        std::cerr << "ERROR: Could not acquire materials, handle does not exist" << std::endl;
        throw std::runtime_error("Missing handle");
    }
    return iter->second;
}

ModelHandle TextureManager::assignHandle()
{
    static ModelHandle nextHandle{1};

    ModelHandle handle{nextHandle++};

    materialLookup_[handle];

    return handle;
}

void TextureManager::textureFromAsset(fastgltf::Asset &asset, ModelHandle handle, std::filesystem::path parentPath)
{
    auto materialIter = materialLookup_.find(handle);

    if (materialIter == materialLookup_.end())
    {
        std::cerr << "ERROR: " << parentPath.string() << " attempted to load with no model handle" << std::endl;
        throw std::runtime_error("Missing handle");
    }

    // Get raw image data
    auto paths = fastgltfLoader::getURIs(asset, parentPath);
    ImageLoader imageLoader{threadPool_, paths};
    std::vector<LoadedImage> rawImageData = imageLoader.getImageData();

    // Resize to match fastgltf materials array
    std::vector<Material> &materials{materialIter->second};
    materials.resize(asset.materials.size());

    // Map image index to texture id for deduplication
    std::unordered_map<unsigned int, Texture> imageIndices;

    // Helper function. If the map contains the OpenGL texture ID already, return that. Otherwise, create a new texture
    auto getOrCreateTextureID = [&](const fastgltf::TextureInfo &textureInfo, bool sRGB, bool anisotropic) {
        auto [imageIndex, sampler] = fastgltfLoader::getTextureInfo(asset, textureInfo);
        auto [index, inserted] = imageIndices.try_emplace(imageIndex, 0);

        if (inserted)
        {
            // Assume all GLTF models get mipmaps and generate them after the arrayTexture is built
            TextureRequest request{sRGB, false, anisotropic ? globalAnisotropicDegree_ : 0.0f};

            // Generate a new texture
            Texture texture{textureUtilities::generateTexture(rawImageData[imageIndex], sampler, request)};

            // Determine which sampler2dArray to use, and get depth
            texture.arrayEntry = assignToTextureArray(texture);

            // Insert the new texture at the image index
            index->second = texture;
        }

        return index->second;
    };

    // Process all materials in the gltf asset
    for (size_t i{0}; i < asset.materials.size(); ++i)
    {
        const fastgltf::Material &assetMaterial = asset.materials[i];
        Material material;

        if (assetMaterial.pbrData.baseColorTexture.has_value())
        {
            // Albedo textures always get sRGB. Try to force aniso and let the global setting resolve whether it gets
            // applied
            const fastgltf::TextureInfo &albedoInfo{assetMaterial.pbrData.baseColorTexture.value()};

            // Deduplicate texture
            Texture albedo{getOrCreateTextureID(albedoInfo, true, true)};

            // Callers only know of a depth and which sampler2Darray to choose
            material.albedo = albedo.arrayEntry;
        }

        if (assetMaterial.pbrData.metallicRoughnessTexture.has_value())
        {
            // ARM map never gets sRGB or anisotropic filtering
            const fastgltf::TextureInfo &armInfo = assetMaterial.pbrData.metallicRoughnessTexture.value();
            Texture ARM{getOrCreateTextureID(armInfo, false, false)};

            material.ARM = ARM.arrayEntry;
        }

        if (assetMaterial.normalTexture.has_value())
        {
            // Normal maps get no sRGB or aniso
            const fastgltf::TextureInfo &normalInfo = assetMaterial.normalTexture.value();
            Texture normal{getOrCreateTextureID(normalInfo, false, false)};

            material.normal = normal.arrayEntry;
        }

        materialLookup_[handle][i] = material;
    }
}

void TextureManager::cubemapFromPaths(ModelHandle handle, const std::vector<std::filesystem::path> &imagePaths,
                                      bool mipmaps, bool sRGB)
{
    if (imagePaths.empty())
        return;

    auto materialIter = materialLookup_.find(handle);

    if (materialIter == materialLookup_.end())
    {
        std::cerr << "ERROR: " << imagePaths[0].string() << " attempted to load with no model handle" << std::endl;
        throw std::runtime_error("Missing handle");
    }

    // Assume cubemaps never want anisotropic filtering
    TextureRequest textureDetails{sRGB, mipmaps, 0.0f};

    // Resize materials to 1
    std::vector<Material> &materials{materialIter->second};
    materials.resize(1);

    // Raw data
    ImageLoader imageLoader{threadPool_, imagePaths};
    std::vector<LoadedImage> loadedImages{imageLoader.getImageData()};

    // Create and assign
    Texture cubemapTexture{textureUtilities::generateCubemapTexture(loadedImages, textureDetails)};

    materials[0].cubemap = cubemapTexture.id;
    cubemapIDs_.insert(cubemapTexture.id);
}

void TextureManager::freeArrayTextures(SamplerArrayEntry &entry)
{
    if (entry.layer == -1 || entry.array || arrayTextures_[entry.array].textureIDs[entry.layer] == 0)
        return;

    glDeleteTextures(1, &arrayTextures_[entry.array].textureIDs[entry.layer]);
    arrayTextures_[entry.array].textureIDs[entry.layer] = 0;
}

// Borrow another models materials, completely releasing self's materials
void TextureManager::borrowMaterials(ModelHandle self, ModelHandle other)
{
    auto selfIter = materialLookup_.find(self);
    auto otherIter = materialLookup_.find(other);

    if (selfIter == materialLookup_.end() || otherIter == materialLookup_.end())
    {
        std::cerr << "ERROR: attempted to borrow from non-existing handle" << std::endl;
        throw std::runtime_error("Missing handle");
    }

    std::vector<Material> &selfEntry{selfIter->second};
    std::vector<Material> &otherEntry{otherIter->second};

    for (std::size_t i{0}; i < selfEntry.size() && i < otherEntry.size(); ++i)
    {
        freeArrayTextures(selfEntry[i].albedo);
        freeArrayTextures(selfEntry[i].ARM);
        freeArrayTextures(selfEntry[i].normal);
    }

    selfEntry = otherEntry;

    if (selfEntry.size() != otherEntry.size())
        std::cerr << "WARNING: models borrowing with different material count" << std::endl;
}

// Assumes caller is giving up control of their texture ID, overwriting anything in that slot
void TextureManager::setCubemap(ModelHandle handle, GLuint textureID)
{
    auto materialIter = materialLookup_.find(handle);

    if (materialIter == materialLookup_.end())
    {
        std::cerr << "ERROR: non-existant handle" << std::endl;
        throw std::runtime_error("Missing handle");
    }

    std::vector<Material> &materials{materialIter->second};

    for (Material &material : materials)
    {
        if (cubemapIDs_.contains(material.cubemap))
        {
            glDeleteTextures(1, &material.cubemap);
            cubemapIDs_.erase(material.cubemap);
        }

        material.cubemap = textureID;
    }

    cubemapIDs_.insert(textureID);
}

// Inserts a texture into a sampler2DArray and returns an array identifier and depth
SamplerArrayEntry TextureManager::assignToTextureArray(Texture &texture)
{
    for (std::size_t i{0}; i < arrayTextures_.size(); ++i)
    {
        if (arrayTextures_[i].width == texture.width && arrayTextures_[i].internalFormat == texture.internalFormat)
        {
            arrayTextures_[i].textureIDs.push_back(texture.id);
            int layer = static_cast<int>(arrayTextures_[i].textureIDs.size() - 1);
            return {static_cast<int>(i), layer};
        }
    }

    std::cerr << "No matching array for " + std::to_string(texture.width) + " format " +
                     std::to_string(texture.internalFormat)
              << std::endl;
    throw std::runtime_error("ArrayTexture bucket not found");
}

void TextureManager::borrowCubemaps(ModelHandle self, ModelHandle other)
{
    auto selfIter = materialLookup_.find(self);
    auto otherIter = materialLookup_.find(other);

    if (selfIter == materialLookup_.end() || otherIter == materialLookup_.end())
    {
        std::cerr << "ERROR: attempted to borrow from non-existing handle" << std::endl;
        throw std::runtime_error("Missing handle");
    }

    std::vector<Material> &selfEntry{selfIter->second};
    std::vector<Material> &otherEntry{otherIter->second};

    for (std::size_t i{0}; i < selfEntry.size() && i < otherEntry.size(); ++i)
    {

        if (cubemapIDs_.contains(selfEntry[i].cubemap))
        {
            glDeleteTextures(1, &selfEntry[i].cubemap);
            cubemapIDs_.erase(selfEntry[i].cubemap);
        }

        selfEntry[i].cubemap = otherEntry[i].cubemap;
    }
}

void TextureManager::buildArrayTextures()
{
    int texturesCopied{0};

    for (std::size_t i{0}; i < arrayTextures_.size(); ++i)
    {
        if (arrayTextures_[i].textureIDs.empty())
            continue;

        int mipLevels{textureUtilities::calculateMipmapLevels(arrayTextures_[i].width, arrayTextures_[i].width)};
        GLuint &id{arrayTextures_[i].arrayTextureID};
        int width{arrayTextures_[i].width};

        glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &id);
        glTextureStorage3D(id, mipLevels, arrayTextures_[i].internalFormat, width, width,
                           static_cast<GLsizei>(arrayTextures_[i].textureIDs.size()));

        glTextureParameteri(id, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(id, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (globalAnisotropicDegree_ > 0.0f)
            glTextureParameterf(id, GL_TEXTURE_MAX_ANISOTROPY, globalAnisotropicDegree_);

        for (std::size_t layer{0}; layer < arrayTextures_[i].textureIDs.size(); ++layer)
        {
            glCopyImageSubData(arrayTextures_[i].textureIDs[layer], GL_TEXTURE_2D, 0, 0, 0, 0, id, GL_TEXTURE_2D_ARRAY,
                               0, 0, 0, static_cast<GLint>(layer), width, width, 1);

            ++texturesCopied;
            glDeleteTextures(1, &arrayTextures_[i].textureIDs[layer]);

            // Flush any old textures from memory to keep VRAM down
            if (texturesCopied % 4 == 0)
                glFinish();
        }

        glGenerateTextureMipmap(id);
        glBindTextureUnit(static_cast<GLuint>(i), id);
    }

    glFinish();
}
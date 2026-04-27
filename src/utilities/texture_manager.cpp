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
#include "graphics_settings.h"
#include "image_loader.h"
#include "render_target.h"
#include "texture_utilities.h"
#include "textures.h"
#include "thread_pool.h"

TextureManager::TextureManager(ThreadPool &threadPool, const GraphicsSettings &settings, int reflectionResolution)
    : threadPool_{threadPool}, globalAnisotropicDegree_{static_cast<float>(settings.anisotropicFiltering)}
{
    bool bucketFound{false};

    for (ArrayTexture &texture : cubemapArrayTextures_)
    {
        if (texture.width == reflectionResolution && texture.internalFormat == dynamicCubemapInternalFormat_)
        {
            bucketFound = true;
            break;
        }
    }

    if (!bucketFound)
        cubemapArrayTextures_.back() = {
            .width = reflectionResolution,
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

void TextureManager::loadModelTextures(const ModelHandle handle, fastgltf::Asset &&asset,
                                       const std::filesystem::path &parentPath)
{
    texturesFromAsset(asset, handle, parentPath);
}

void TextureManager::loadCubemapTextures(const ModelHandle handle, std::vector<std::filesystem::path> imagePaths,
                                         const std::filesystem::path &parentPath, bool mipmaps)
{
    cubemapFromPaths(handle, std::move(imagePaths), mipmaps);
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

// Returns the internal color format used for dynamic cubemaps
GLenum TextureManager::getReflectionColorFormat() const
{
    return dynamicCubemapInternalFormat_;
}

ModelHandle TextureManager::assignHandle()
{
    static ModelHandle nextHandle{1};

    ModelHandle handle{nextHandle++};

    materialLookup_[handle];

    return handle;
}

void TextureManager::texturesFromAsset(fastgltf::Asset &asset, ModelHandle handle, std::filesystem::path parentPath)
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
            // Let mipmaps get built by the array textures
            TextureDetails request{
                .sRGB = sRGB,
                .mipmaps = false,
                .anisotropic = anisotropic ? globalAnisotropicDegree_ : 0.0f,
            };

            Texture texture{textureUtilities::generateTexture(rawImageData[imageIndex], sampler, request)};

            if (texture.width != texture.height)
            {
                std::cerr << "ERROR: Non square texture from " << parentPath << std::endl;
                throw std::runtime_error("Non square texture");
            }

            // Determine which sampler2dArray to use, and get the array depth
            texture.arrayEntry = assignToTextureArray(texture);

            // Insert the new texture at the image index
            index->second = texture;
        }

        return index->second;
    };

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

        materials[i] = material;
    }
}

void TextureManager::cubemapFromPaths(ModelHandle handle, const std::vector<std::filesystem::path> &imagePaths,
                                      bool mipmaps)
{
    if (imagePaths.empty())
        return;

    auto materialIter = materialLookup_.find(handle);

    if (materialIter == materialLookup_.end())
    {
        std::cerr << "ERROR: " << imagePaths[0].string() << " attempted to load with no model handle" << std::endl;
        throw std::runtime_error("Missing handle");
    }

    // Assume cubemaps are always skyboxes or reflections - no aniso and always sRGB
    TextureDetails textureDetails{true, mipmaps, 0.0f};

    // Resize materials to 1
    std::vector<Material> &materials{materialIter->second};
    materials.resize(1);

    // Raw data
    ImageLoader imageLoader{threadPool_, imagePaths};
    std::vector<LoadedImage> loadedImages{imageLoader.getImageData()};

    // Create and assign
    Texture cubemapTexture{textureUtilities::generateCubemapTexture(loadedImages, textureDetails)};
    cubemapTexture.arrayEntry = assignToCubeArray(cubemapTexture);

    materials.front().cubemap = cubemapTexture.arrayEntry;
}

// Borrow another model's materials. Callers should verify that "self" has not loaded any textures, as they will be
// orphaned
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

    selfEntry = otherEntry;

    if (selfEntry.size() != otherEntry.size())
        std::cerr << "WARNING: models borrowing with different material count" << std::endl;
}

// Overwrites any existing cubemaps for this model
void TextureManager::setCubemap(ModelHandle handle, RenderTarget &renderTarget, SamplerArrayEntry arrayEntry)
{
    auto materialIter = materialLookup_.find(handle);

    if (materialIter == materialLookup_.end())
    {
        std::cerr << "ERROR: non-existant handle" << std::endl;
        throw std::runtime_error("Missing handle");
    }

    ArrayTexture &arrayTexture{cubemapArrayTextures_[arrayEntry.array]};

    if (arrayTexture.id == 0)
        return;

    int width{arrayTexture.width};
    GLuint sourceTextureID{renderTarget.getColorTextureID()};

    glCopyImageSubData(sourceTextureID, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, arrayTexture.id, GL_TEXTURE_CUBE_MAP_ARRAY, 0,
                       0, 0, static_cast<GLint>(arrayEntry.layer * 6), width, width, 6);

    glGenerateTextureMipmap(arrayTexture.id);

    for (Material &material : materialIter->second)
    {
        if (material.cubemap.array != -1 || material.cubemap.layer != -1)
            std::cerr << "Warning: Orphaning cubemap for handle " << handle << std::endl;

        material.cubemap = arrayEntry;
    }
}

// Inserts a texture into a sampler2DArray and returns an array identifier and depth
SamplerArrayEntry TextureManager::assignToTextureArray(Texture &texture)
{
    for (std::size_t i{0}; i < arrayTextures_.size(); ++i)
    {
        if (texture.width == arrayTextures_[i].width && texture.internalFormat == arrayTextures_[i].internalFormat)
        {
            pending2dTextures_.emplace_back(PendingTexture{
                .id = texture.id,
                .arrayEntry =
                    {
                        .array = static_cast<int>(i),
                        .layer = arrayTextures_[i].count,
                    },
            });

            return {static_cast<int>(i), arrayTextures_[i].count++};
        }
    }

    std::cerr << "No matching 2d array for " + std::to_string(texture.width) << " format "
              << std::to_string(texture.internalFormat) << std::endl;
    throw std::runtime_error("ArrayTexture bucket not found");
}

SamplerArrayEntry TextureManager::assignToCubeArray(Texture &texture)
{
    for (std::size_t i{0}; i < cubemapArrayTextures_.size(); ++i)
    {
        if (texture.width == cubemapArrayTextures_[i].width &&
            texture.internalFormat == cubemapArrayTextures_[i].internalFormat)
        {

            pendingCubemaps_.emplace_back(PendingTexture{
                .id = texture.id,
                .arrayEntry =
                    {
                        .array = static_cast<int>(i),
                        .layer = cubemapArrayTextures_[i].count,
                    },
            });

            return {static_cast<int>(i), cubemapArrayTextures_[i].count++};
        }
    }

    std::cerr << "No matching cubemap array for " + std::to_string(texture.width) << " format "
              << std::to_string(texture.internalFormat) << std::endl;
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
        if (selfEntry[i].cubemap.array != -1 || selfEntry[i].cubemap.layer != -1)
            std::cerr << "Warning: Orphaning cubemap for handle " << self << std::endl;

        selfEntry[i].cubemap = otherEntry[i].cubemap;
    }
}

void TextureManager::finalize2dAllocations()
{
    if (textures2dAllocated_)
    {
        std::cerr << "ERROR: 2D textures already allocated" << std::endl;
        throw std::runtime_error("Texture allocation error");
    }

    for (std::size_t i{0}; i < arrayTextures_.size(); ++i)
    {
        if (!arrayTextures_[i].count)
            continue;

        ArrayTexture &arrayTexture{arrayTextures_[i]};
        GLuint &arrayTexID{arrayTexture.id};
        GLsizei numTextures{static_cast<GLsizei>(arrayTexture.count)};

        int width{arrayTexture.width};
        int mipLevels{textureUtilities::calculateMipmapLevels(width, width)};

        glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &arrayTexID);
        glTextureStorage3D(arrayTexID, mipLevels, arrayTexture.internalFormat, width, width, numTextures);

        glTextureParameteri(arrayTexID, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(arrayTexID, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTextureParameteri(arrayTexID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(arrayTexID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        if (globalAnisotropicDegree_ > 0.0f)
            glTextureParameterf(arrayTexID, GL_TEXTURE_MAX_ANISOTROPY, globalAnisotropicDegree_);

        glBindTextureUnit(static_cast<GLuint>(i), arrayTexID);
    }

    textures2dAllocated_ = true;
}

void TextureManager::build2dTextures()
{
    if (pending2dTextures_.empty())
        return;

    int texturesCopied{0};

    for (PendingTexture &pendingTexture : pending2dTextures_)
    {
        ArrayTexture &arrayTexture{arrayTextures_[pendingTexture.arrayEntry.array]};

        if (arrayTexture.id == 0 || pendingTexture.id == 0)
            continue;

        int width{arrayTexture.width};

        glCopyImageSubData(pendingTexture.id, GL_TEXTURE_2D, 0, 0, 0, 0, arrayTexture.id, GL_TEXTURE_2D_ARRAY, 0, 0, 0,
                           static_cast<GLint>(pendingTexture.arrayEntry.layer), width, width, 1);

        ++texturesCopied;
        glDeleteTextures(1, &pendingTexture.id);
        pendingTexture.id = 0;

        // Flush any old textures from memory to keep VRAM down
        if (texturesCopied % 4 == 0)
            glFinish();
    }

    for (ArrayTexture &texture : arrayTextures_)
        if (texture.id != 0)
            glGenerateTextureMipmap(texture.id);

    // Force cleanup of any textures flagged for deletion
    glFinish();
}

void TextureManager::buildCubemapTextures()
{
    if (pendingCubemaps_.empty())
        return;

    for (PendingTexture &pendingTexture : pendingCubemaps_)
    {
        ArrayTexture &arrayTexture{cubemapArrayTextures_[pendingTexture.arrayEntry.array]};

        if (arrayTexture.id == 0)
            continue;

        if (pendingTexture.id == 0)
            continue;

        int width{arrayTexture.width};

        glCopyImageSubData(pendingTexture.id, GL_TEXTURE_CUBE_MAP, 0, 0, 0, 0, arrayTexture.id,
                           GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, static_cast<GLint>(pendingTexture.arrayEntry.layer * 6),
                           width, width, 6);

        glDeleteTextures(1, &pendingTexture.id);
        pendingTexture.id = 0;
    }

    for (ArrayTexture &texture : cubemapArrayTextures_)
        if (texture.id != 0)
            glGenerateTextureMipmap(texture.id);

    glFinish();
}

SamplerArrayEntry TextureManager::reserveDynamicCubemaps(int numCubemaps, int resolution, GLenum internalFormat)
{
    for (std::size_t i{0}; i < cubemapArrayTextures_.size(); ++i)
    {
        if (cubemapArrayTextures_[i].width == resolution && cubemapArrayTextures_[i].internalFormat == internalFormat)
        {
            int startingLayer{cubemapArrayTextures_[i].count};
            cubemapArrayTextures_[i].count += numCubemaps;

            return {
                .array = static_cast<int>(i),
                .layer = startingLayer,
            };
        }
    }

    std::cerr << "No matching cubemap array for " << resolution << ", format " << std::to_string(internalFormat)
              << std::endl;
    throw std::runtime_error("ArrayTexture bucket not found");
}

void TextureManager::finalizeCubemapAllocations()
{
    if (texturesCubemapAllocated_)
    {
        std::cerr << "ERROR: Cubemaps textures already allocated" << std::endl;
        throw std::runtime_error("Texture allocation error");
    }

    for (std::size_t i{0}; i < cubemapArrayTextures_.size(); ++i)
    {
        if (!cubemapArrayTextures_[i].count)
            continue;

        ArrayTexture &arrayTexture{cubemapArrayTextures_[i]};
        GLuint &arrayTexID{arrayTexture.id};
        GLsizei numTextures{static_cast<GLsizei>(arrayTexture.count)};

        int width{arrayTexture.width};
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

        constexpr GLuint cubemapUnitOffset{static_cast<GLuint>(TextureUnit::cubemapSRGB4k)};
        glBindTextureUnit(static_cast<GLuint>(cubemapUnitOffset + i), arrayTexID);
    }

    texturesCubemapAllocated_ = true;
}
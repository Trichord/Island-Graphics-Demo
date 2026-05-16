#pragma once

#include <filesystem>

#include <vector>

#include "fastgltf/types.hpp"
#include "handle.h"
#include "primitive.h"
#include "textures.h"

namespace AssetLoader
{

struct TextureDescriptor
{
    Sampler sampler;
    std::size_t imageIndex{0};
};

struct DeferredTexture
{
    Handle handle{0};
    std::filesystem::path path;

    bool textureRepeat{true};
    std::vector<int> materialIndices;
    Material::Type type{Material::Type::albedo};
};

fastgltf::Asset loadAsset(const std::filesystem::path &path);
std::vector<Primitive> loadMeshes(const fastgltf::Asset &asset);
std::vector<std::filesystem::path> getURIs(const fastgltf::Asset &asset, const std::filesystem::path &parentPath);
TextureDescriptor getTextureInfo(const fastgltf::Asset &asset, const fastgltf::TextureInfo &texInfo);
std::vector<DeferredTexture> texturesFromAsset(fastgltf::Asset &asset, const std::filesystem::path &parentPath,
                                               Handle handle);
} // namespace AssetLoader
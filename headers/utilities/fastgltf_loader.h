#pragma once

#include <filesystem>
#include <utility>
#include <vector>

#include "fastgltf/types.hpp"
#include "primitive.h"
#include "texture_utilities.h"
#include "textures.h"

namespace fastgltfLoader
{
fastgltf::Asset loadAsset(const std::filesystem::path &path);
std::vector<Primitive> loadMeshes(const fastgltf::Asset &asset);
std::vector<std::filesystem::path> getURIs(const fastgltf::Asset &asset, const std::filesystem::path &parentPath);
std::pair<unsigned int, Sampler> getTextureInfo(const fastgltf::Asset &asset, const fastgltf::TextureInfo &texInfo);
} // namespace fastgltfLoader
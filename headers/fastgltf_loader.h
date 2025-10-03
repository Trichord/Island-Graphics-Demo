#pragma once

#include <fastgltf/types.hpp>
#include <vector>

#include "primitive.h"
#include "texture_utilities.h"

namespace fastgltf_loader {
fastgltf::Asset loadAsset(std::filesystem::path path);
std::vector<Primitive::Mesh> loadMeshes(const fastgltf::Asset &asset);
std::vector<std::filesystem::path> getURIs(
    const fastgltf::Asset &asset, const std::filesystem::path parentPath);
std::pair<unsigned int, Sampler> getTextureInfo(
    const fastgltf::Asset &asset, const fastgltf::TextureInfo &texInfo);
}  // namespace fastgltf_loader
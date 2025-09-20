#pragma once

#include "primitive.h"
#include "texture_manager.h"
#include <vector>
#include <fastgltf/types.hpp>

namespace fastgltf_loader {

	fastgltf::Asset loadAsset(std::filesystem::path path);
	std::vector<Primitive::Mesh> loadMeshes(const fastgltf::Asset& asset);
	std::vector<std::filesystem::path> getURIs(const fastgltf::Asset& asset, const std::filesystem::path parentPath);
	std::pair<unsigned int, Sampler> getTextureInfo(const fastgltf::Asset& asset, const fastgltf::TextureInfo& texInfo);
}
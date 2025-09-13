#pragma once

#include "primitive.h"
#include <vector>
#include <fastgltf/types.hpp>

fastgltf::Asset fastgltfLoadAsset(std::filesystem::path path);
std::vector<Primitive::Mesh> fastgltfLoadMeshes(const fastgltf::Asset& asset);
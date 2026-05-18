#include "model.h"

#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "aabb.h"
#include "asset_loader.h"
#include "fastgltf/types.hpp"
#include "glm/glm.hpp"
#include "primitive.h"

#include "texture_manager.h"

Model::Model(const std::filesystem::path &path)
    : asset_{AssetLoader::loadAsset(path)}, filePath_{path}, primitives_{AssetLoader::loadMeshes(asset_)},
      aabb_{calculateAABB()}
{
}

const AABB &Model::getAABB() const
{
    return aabb_;
}

const std::vector<Primitive> &Model::getPrimitives() const
{
    return primitives_;
}

const fastgltf::Asset &Model::getAsset() const
{
    return asset_;
}

const std::filesystem::path &Model::getPath() const
{
    return filePath_;
}

AABB Model::calculateAABB() const
{
    glm::vec3 min{std::numeric_limits<float>::infinity()};
    glm::vec3 max{-std::numeric_limits<float>::infinity()};

    for (const Primitive &primitive : primitives_)
    {
        min = glm::min(min, primitive.aabb.getMin());
        max = glm::max(max, primitive.aabb.getMax());
    }

    return {min, max};
}
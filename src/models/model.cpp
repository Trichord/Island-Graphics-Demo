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
#include "fastgltf/types.hpp"
#include "asset_loader.h"
#include "glm/glm.hpp"
#include "primitive.h"

#include "texture_manager.h"

Model::Model(TextureManager &textureManager, const std::filesystem::path &path, std::optional<Handle> borrowHandle)
    : handle_{textureManager.assignHandle()}, filePath_{path}
{
    fastgltf::Asset asset = AssetLoader::loadAsset(path);
    primitives_ = AssetLoader::loadMeshes(asset);
    calculateAABB();

    textureManager.loadModelTextures(handle_, std::move(asset), path.parent_path(), borrowHandle);
}

AABB &Model::getAABB()
{
    return aabb_;
}

std::span<Primitive> Model::getPrimitives()
{
    return primitives_;
}

std::optional<Handle> Model::getBorrowedHandle() const
{
    return borrowedHandle_;
}

Handle Model::getHandle() const
{
    return handle_;
}

void Model::calculateAABB()
{
    glm::vec3 min{std::numeric_limits<float>::infinity()};
    glm::vec3 max{-std::numeric_limits<float>::infinity()};

    for (const Primitive &primitive : primitives_)
    {
        min = glm::min(min, primitive.aabb.getMin());
        max = glm::max(max, primitive.aabb.getMax());
    }

    aabb_ = AABB{min, max};
}

std::optional<std::vector<Primitive>> Model::loadCollisionMesh() const
{
    std::filesystem::path collisionMeshPath{filePath_.parent_path() / "collision"};
    std::string gltf{filePath_.stem().string() + "_collision.gltf"};
    std::filesystem::path fullPath{collisionMeshPath / gltf};

    std::error_code error;

    if (std::filesystem::exists(fullPath, error))
    {
        fastgltf::Asset asset = AssetLoader::loadAsset(fullPath);
        return AssetLoader::loadMeshes(asset);
    }
    else if (error)
    {
        std::cerr << error.message() << std::endl;
        throw std::runtime_error("ERROR: Unable to load collision mesh.");
    }
    else
        return {};
}
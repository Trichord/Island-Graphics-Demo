#include "base_model.h"

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
#include "fastgltf_loader.h"
#include "glm/glm.hpp"
#include "primitive.h"

#include "texture_manager.h"

BaseModel::BaseModel(TextureManager &textureManager, const std::filesystem::path &path)
    : handle_{textureManager.assignHandle()}, textureManager_{textureManager}, filePath_{path}
{
    loadModel(path, false);
    calculateAABB();
}

BaseModel::BaseModel(TextureManager &textureManager, const ModelHandle borrowHandle, const std::filesystem::path &path)
    : handle_{textureManager.assignHandle()}, textureManager_{textureManager}, filePath_{path}
{
    textureManager.borrowMaterials(handle_, borrowHandle);
    loadModel(path, true);
    calculateAABB();
}

AABB &BaseModel::getAABB()
{
    return aabb_;
}

std::span<Primitive> BaseModel::getPrimitives()
{
    return primitives_;
}

void BaseModel::loadModel(const std::filesystem::path &path, bool borrowed)
{
    fastgltf::Asset asset = fastgltfLoader::loadAsset(path);

    primitives_ = fastgltfLoader::loadMeshes(asset);

    if (!borrowed)
        textureManager_.loadModelTextures(handle_, std::move(asset), path.parent_path());
}

ModelHandle BaseModel::getHandle() const
{
    return handle_;
}

void BaseModel::calculateAABB()
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

std::optional<std::vector<Primitive>> BaseModel::loadCollisionMesh() const
{
    std::filesystem::path collisionMeshPath{filePath_.parent_path() / "collision"};
    std::string gltf{filePath_.stem().string() + "_collision.gltf"};
    std::filesystem::path fullPath{collisionMeshPath / gltf};

    std::error_code error;

    if (std::filesystem::exists(fullPath, error))
    {
        fastgltf::Asset asset = fastgltfLoader::loadAsset(fullPath);
        return fastgltfLoader::loadMeshes(asset);
    }
    else if (error)
    {
        std::cerr << error.message() << std::endl;
        throw std::runtime_error("ERROR: Unable to load collision mesh.");
    }
    else
        return {};
}
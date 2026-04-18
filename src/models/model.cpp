#include "model.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "aabb.h"
#include "fastgltf/types.hpp"
#include "fastgltf_loader.h"
#include "glm/glm.hpp"
#include "primitive.h"
#include "renderable.h"
#include "texture_manager.h"

Model::Model(TextureManager &textureManager, const std::filesystem::path &path)
    : handle_{textureManager.assignHandle()}, textureManager_{textureManager}, filePath_{path}
{
    loadModel(path, false);
    calculateAABB();
}

Model::Model(TextureManager &textureManager, const ModelHandle borrowHandle, const std::filesystem::path &path)
    : handle_{textureManager.assignHandle()}, textureManager_{textureManager}, filePath_{path}
{
    textureManager.borrowMaterials(handle_, borrowHandle);
    loadModel(path, true);
    calculateAABB();
}

Model::Model(Model &&other) noexcept
    : handle_{other.handle_}, textureManager_{other.textureManager_}, meshes_{std::move(other.meshes_)},
      aabb_{std::move(other.aabb_)}
{
    other.handle_ = 0;
}

AABB &Model::getAABB()
{
    return aabb_;
}

std::span<Primitive::Mesh> Model::getMeshes()
{
    return meshes_;
}

std::optional<std::vector<BoundedTransform>> Model::loadInstancedTransforms()
{
    return std::nullopt;
}

void Model::loadModel(const std::filesystem::path &path, bool borrowed)
{
    fastgltf::Asset asset = fastgltfLoader::loadAsset(path);

    meshes_ = fastgltfLoader::loadMeshes(asset);

    if (!borrowed)
        textureManager_.loadModelTextures(handle_, std::move(asset), path.parent_path());
}

ModelHandle Model::getHandle() const
{
    return handle_;
}

void Model::calculateAABB()
{
    glm::vec3 min{std::numeric_limits<float>::infinity()};
    glm::vec3 max{-std::numeric_limits<float>::infinity()};

    for (const auto &mesh : meshes_)
        for (const auto &primitive : mesh.primitives)
        {
            const AABB &primAABB{primitive.getAABB()};

            min = glm::min(min, primAABB.getMin());
            max = glm::max(max, primAABB.getMax());
        }

    aabb_ = AABB{min, max};
}

void Model::setMeshVisibility(bool visible)
{
    for (auto &mesh : meshes_)
        mesh.isVisible = visible;
}

std::optional<std::vector<Primitive::Mesh>> Model::loadCollisionMesh()
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
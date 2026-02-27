#include "model.h"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "aabb.h"
#include "fastgltf/types.hpp"
#include "fastgltf_loader.h"
#include "glm/glm.hpp"
#include "instance_data.h"
#include "primitive.h"
#include "texture_manager.h"

Model::Model(TextureManager &textureManager, const std::filesystem::path &path)
    : handle_{textureManager.assignHandle()}, textureManager_{textureManager}, filePath_{path}
{
    loadModel(path, false);
    calculateAABB();
}

Model::Model(TextureManager &textureManager, const ModelHandle borrowHandle, const std::filesystem::path &path)
    : handle_{textureManager.assignHandle()}, textureManager_{textureManager}
{
    textureManager.borrowAssignMaterials(handle_, borrowHandle);
    loadModel(path, true);
    calculateAABB();
}

Model::Model(Model &&other) noexcept
    : handle_{other.handle_}, textureManager_{other.textureManager_}, meshes_{std::move(other.meshes_)},
      aabb_{std::move(other.aabb_)}
{
    other.handle_ = 0;
}

Model::~Model()
{
    if (handle_)
        textureManager_.releaseHandle(handle_);
}

AABB &Model::getAABB()
{
    return aabb_;
}

std::span<Primitive::Mesh> Model::getMeshes()
{
    return meshes_;
}

std::optional<std::vector<InstancedData>> Model::generateInstancedData()
{
    return std::nullopt;
}

void Model::loadModel(const std::filesystem::path &path, bool borrowed)
{
    fastgltf::Asset asset = fastgltfLoader::loadAsset(path);

    meshes_ = fastgltfLoader::loadMeshes(asset);

    if (!borrowed)
        textureManager_.loadModel(handle_, std::move(asset), path.parent_path());
}

const ModelHandle Model::getHandle() const
{
    return handle_;
}

void Model::assignMaterialIndex(std::size_t newIndex)
{
    for (auto &mesh : meshes_)
        for (auto &primitive : mesh.primitives)
            primitive.setMaterialIndex(newIndex);
}

bool Model::hasMaterials() const
{
    for (const auto &mesh : meshes_)
        for (const auto &primitive : mesh.primitives)
            if (primitive.materialsExist())
                return true;

    return false;
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

void Model::setVisibility(bool visible)
{
    for (auto &mesh : meshes_)
        mesh.isVisible = visible;
}
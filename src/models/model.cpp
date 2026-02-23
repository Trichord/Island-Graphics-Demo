#include "model.h"

#include <algorithm>
#include <filesystem>
#include <limits>
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
    : handle{textureManager.assignHandle()}, texManager{textureManager}
{
    loadModel(path, false);
    calculateAABB();
}

Model::Model(TextureManager &textureManager, const ModelHandle borrowHandle, const std::filesystem::path &path)
    : handle{textureManager.assignHandle()}, texManager{textureManager}
{
    textureManager.borrowAssignMaterials(handle, borrowHandle);
    loadModel(path, true);
    calculateAABB();
}

Model::Model(Model &&other) noexcept
    : handle{other.handle}, texManager{other.texManager}, meshes{std::move(other.meshes)}, aabb{std::move(other.aabb)}
{
    other.handle = 0;
}

Model::~Model()
{
    if (handle)
        texManager.releaseHandle(handle);
}

AABB &Model::getAABB()
{
    return aabb;
}

std::span<Primitive::Mesh> Model::getMeshes()
{
    return meshes;
}

std::span<InstancedData> Model::getInstanceData()
{
    return {};
}

void Model::loadModel(const std::filesystem::path &path, bool borrowed)
{
    fastgltf::Asset asset = fastgltfLoader::loadAsset(path);

    meshes = fastgltfLoader::loadMeshes(asset);

    if (!borrowed)
        texManager.loadModel(handle, std::move(asset), path.parent_path());
}

const ModelHandle Model::getHandle() const
{
    return handle;
}

void Model::assignMaterialIndex(std::size_t newIndex)
{
    for (auto &mesh : meshes)
        for (auto &primitive : mesh.primitives)
            primitive.setMaterialIndex(newIndex);
}

bool Model::hasMaterials() const
{
    for (const auto &mesh : meshes)
        for (const auto &primitive : mesh.primitives)
            if (primitive.materialsExist())
                return true;

    return false;
}

void Model::calculateAABB()
{
    float minX{std::numeric_limits<float>::infinity()};
    float minY{std::numeric_limits<float>::infinity()};
    float minZ{std::numeric_limits<float>::infinity()};
    float maxX{-std::numeric_limits<float>::infinity()};
    float maxY{-std::numeric_limits<float>::infinity()};
    float maxZ{-std::numeric_limits<float>::infinity()};

    for (const auto &mesh : meshes)
        for (const auto &primitive : mesh.primitives)
        {
            const AABB &primAABB{primitive.getAABB()};

            minX = std::min(primAABB.getMinX(), minX);
            minY = std::min(primAABB.getMinY(), minY);
            minZ = std::min(primAABB.getMinZ(), minZ);

            maxX = std::max(primAABB.getMaxX(), maxX);
            maxY = std::max(primAABB.getMaxY(), maxY);
            maxZ = std::max(primAABB.getMaxZ(), maxZ);
        }

    aabb = AABB{minX, minY, minZ, maxX, maxY, maxZ};
}

void Model::setVisibility(bool visible)
{
    for (auto &mesh : meshes)
        mesh.isVisible = visible;
}
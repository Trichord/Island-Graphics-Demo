#include "model.h"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <utility>
#include <vector>

#include "aabb.h"
#include "fastgltf/types.hpp"
#include "fastgltf_loader.h"
#include "glm/glm.hpp"
#include "primitive.h"
#include "texture_manager.h"

Model::Model(TextureManager& textureManager, const std::filesystem::path& path)
    : handle{textureManager.assignModelHandle()}, texManager{&textureManager} {
  loadModel(path, false);
  calculateAABB();
}

Model::Model(TextureManager& textureManager, const Model& borrowTarget,
             const std::filesystem::path& path)
    : handle{borrowTarget.handle}, texManager{&textureManager} {
  loadModel(path, true);
  calculateAABB();
}

Model::Model(Model&& other) noexcept
    : handle{other.handle},
      texManager{other.texManager},
      meshes{std::move(other.meshes)},
      aabb{std::move(other.aabb)} {
  other.handle = 0;
}

Model::~Model() {
  if (handle) texManager->releaseModel(handle);
}

AABB& Model::getAABB() { return aabb; }

std::span<Primitive::Mesh> Model::getMeshes() { return meshes; }

std::optional<std::span<InstanceData>> Model::getInstanceData() {
  return std::nullopt;
}

std::size_t Model::getInstanceCount() const { return 0; }

void Model::loadModel(const std::filesystem::path& path, bool borrowed) {
  fastgltf::Asset asset = fastgltfLoader::loadAsset(path);

  meshes.resize(asset.meshes.size());
  meshes = fastgltfLoader::loadMeshes(asset);

  if (!borrowed)
    texManager->queueModelMaterials(handle, std::move(asset),
                                    path.parent_path());
}

const ModelHandle Model::getModelHandle() const { return handle; }

void Model::assignMaterialIndex(std::size_t newIndex) {
  for (auto& mesh : meshes)
    for (auto& primitive : mesh.primitives)
      primitive.setMaterialIndex(newIndex);
}

bool Model::hasMaterials() const {
  for (const auto& mesh : meshes)
    for (const auto& primitive : mesh.primitives)
      if (primitive.materialsExist()) return true;

  return false;
}

void Model::calculateAABB() {
  float minX{std::numeric_limits<float>::infinity()};
  float minY{std::numeric_limits<float>::infinity()};
  float minZ{std::numeric_limits<float>::infinity()};
  float maxX{-std::numeric_limits<float>::infinity()};
  float maxY{-std::numeric_limits<float>::infinity()};
  float maxZ{-std::numeric_limits<float>::infinity()};

  for (const auto& mesh : meshes)
    for (const auto& primitive : mesh.primitives) {
      const AABB& primAABB{primitive.getAABB()};

      const glm::vec3 center{primAABB.getCenter()};
      const glm::vec3 extent{primAABB.getExtent()};
      const glm::vec3 aabbMin{center - extent};
      const glm::vec3 aabbMax{center + extent};

      minX = std::min(aabbMin.x, minX);
      minY = std::min(aabbMin.y, minY);
      minZ = std::min(aabbMin.z, minZ);

      maxX = std::max(aabbMax.x, maxX);
      maxY = std::max(aabbMax.y, maxY);
      maxZ = std::max(aabbMax.z, maxZ);
    }

  aabb = AABB{minX, minY, minZ, maxX, maxY, maxZ};
}

void Model::sortInstances() { return; }

void Model::resetVisibility() {
  for (auto& mesh : meshes) mesh.isVisible = true;
}
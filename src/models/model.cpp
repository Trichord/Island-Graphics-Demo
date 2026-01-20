#include "model.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>
#include <utility>
#include <vector>

#include "aabb.h"
#include "fastgltf/types.hpp"
#include "fastgltf_loader.h"
#include "gl_state.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "primitive.h"
#include "texture_manager.h"
#include "material_slots.h"

Model::Model(TextureManager& textureManager, const std::filesystem::path& path)
    : handle{textureManager.assignModelHandle()},
      texManager{textureManager},
      materials{textureManager.acquireUniqueTextureSet(handle)} {
  loadModel(path, false);
  calculateAABB();
}

Model::Model(TextureManager& textureManager, const Model& borrowTarget,
             const std::filesystem::path& path)
    : handle{borrowTarget.handle},
      texManager{textureManager},
      materials{textureManager.borrowTextureSet(borrowTarget.handle)} {
  loadModel(path, true);
  calculateAABB();
}

Model::Model(Model&& other) noexcept
    : handle{other.handle},
      texManager{other.texManager},
      materials{other.materials},
      meshes{std::move(other.meshes)} {
  other.handle = 0;
}

Model::~Model() {
  if (handle) texManager.releaseModel(handle);
}

void Model::drawPrimitive(const Primitive& primitive) {
  glDrawElements(GL_TRIANGLES, primitive.getIndicesSize(), GL_UNSIGNED_INT, 0);
}

void Model::render(GLState& glState) {
  std::size_t lastMaterialIndex{0};
  bool materialBound{false};

  for (const Primitive::Mesh& mesh : meshes) {
    if (!mesh.isVisible) continue;
    for (const Primitive& primitive : mesh.primitives) {
      glState.setAlphaMode(primitive.getAlphaMode() !=
                           fastgltf::AlphaMode::Opaque);

      if (primitive.materialsExist()) {
        std::size_t materialIndex{primitive.getMaterialIndex()};

        // Switch materials
        if (materialIndex != lastMaterialIndex || !materialBound) {
          lastMaterialIndex = materialIndex;
          materialBound = true;
          bindTextures(glState, materials[materialIndex]);
        }
      }

      glBindVertexArray(primitive.getVAO());
      drawPrimitive(primitive);
    }
  }

  glState.setAlphaMode(false);
}

void Model::bindTextures(GLState& glState, const Material& materials) {
  using enum MaterialSlot;

  if (materials.albedo) glState.bindTexture(ALBEDO, materials.albedo);

  if (materials.occlusionRoughnessMetallic)
    glState.bindTexture(ARM, materials.occlusionRoughnessMetallic);

  if (materials.normal) glState.bindTexture(NORMAL, materials.normal);

  if (materials.cubemap) glState.bindTexture(CUBEMAP, materials.cubemap);
}

void Model::loadModel(const std::filesystem::path& path, bool borrowed) {
  fastgltf::Asset asset = fastgltfLoader::loadAsset(path);

  meshes.resize(asset.meshes.size());
  meshes = fastgltfLoader::loadMeshes(asset);

  if (!borrowed)
    texManager.queueModelMaterials(handle, std::move(asset),
                                   path.parent_path());
}

ModelHandle Model::getModelHandle() { return handle; }

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

bool Model::performCulling(const std::array<glm::vec4, 6>& frustumPlanes,
                           const glm::mat4& modelMatrix,
                           const glm::vec3& cameraPos,
                           const float cullDistanceScale) {
  bool visible{false};

  if (aabb.intersects(frustumPlanes, modelMatrix)) {
    visible = true;
    for (auto& mesh : meshes) mesh.isVisible = true;
  }
  return visible;
}
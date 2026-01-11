#include "model.h"

#include <algorithm>
#include <filesystem>
#include <limits>
#include <vector>

#include "fastgltf/types.hpp"
#include "fastgltf_loader.h"
#include "glad/glad.h"
#include "primitive.h"
#include "texture_manager.h"

Model::Model(TextureManager& textureManager, const std::filesystem::path& path)
    : handle{textureManager.assignModelHandle()},
      texManager{textureManager},
      textureSet{textureManager.acquireUniqueTextureSet(handle)} {
  loadModel(path, false);
  calculateAABB();
}

Model::Model(TextureManager& textureManager, const Model& borrowTarget,
             const std::filesystem::path& path)
    : handle{borrowTarget.handle},
      texManager{textureManager},
      textureSet{textureManager.borrowTextureSet(borrowTarget.handle)} {
  loadModel(path, true);
  calculateAABB();
}

Model::Model(Model&& other) noexcept
    : handle{other.handle},
      texManager{other.texManager},
      textureSet{other.textureSet},
      meshes{std::move(other.meshes)} {
  other.handle = 0;
}

Model::~Model() {
  if (handle) texManager.releaseModel(handle);
}

void Model::drawPrimitive(const Primitive& primitive) {
  glDrawElements(GL_TRIANGLES, primitive.getIndicesSize(), GL_UNSIGNED_INT, 0);
}

void Model::render() {
  std::size_t lastMaterialIndex{0};
  bool alphaEnabled{false};
  bool needsAlpha{false};
  bool materialBound{false};

  for (const Primitive::Mesh& mesh : meshes) {
    for (const Primitive& primitive : mesh.primitives) {
      needsAlpha = primitive.getAlphaMode() != fastgltf::AlphaMode::Opaque;

      if (primitive.materialsExist()) {
        std::size_t materialIndex{primitive.getMaterialIndex()};

        // Switch materials
        if (materialIndex != lastMaterialIndex || !materialBound) {
          lastMaterialIndex = materialIndex;
          materialBound = true;
          bindTextures(textureSet[materialIndex]);
        }
      }

      // Per-primitive alpha mode
      if (needsAlpha != alphaEnabled) {
        setAlphaMode(needsAlpha);
        alphaEnabled = needsAlpha;
      }

      glBindVertexArray(primitive.getVAO());
      drawPrimitive(primitive);
    }
  }

  if (alphaEnabled) {
    setAlphaMode(false);
  }
}

void Model::loadModel(const std::filesystem::path& path, bool borrowed) {
  fastgltf::Asset asset = fastgltfLoader::loadAsset(path);

  meshes.resize(asset.meshes.size());
  meshes = fastgltfLoader::loadMeshes(asset);

  if (!borrowed)
    texManager.queueModelTextureSet(handle, std::move(asset),
                                    path.parent_path());
}

ModelHandle Model::getModelHandle() { return handle; }

void Model::bindTextures(const TextureSet& textures) {
  if (textures.albedo) glBindTextureUnit(0, textures.albedo);

  if (textures.occlusionRoughnessMetallic)
    glBindTextureUnit(1, textures.occlusionRoughnessMetallic);

  if (textures.normal) glBindTextureUnit(2, textures.normal);

  if (textures.cubemap) glBindTextureUnit(3, textures.cubemap);
}

void Model::setAlphaMode(bool enabled) {
  if (enabled) {
    glDisable(GL_CULL_FACE);
    glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
  } else {
    glEnable(GL_CULL_FACE);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
  }
}

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
      const glm::vec3& aabbMin{primAABB.getMin()};
      const glm::vec3& aabbMax{primAABB.getMax()};

      minX = std::min(aabbMin.x, minX);
      minY = std::min(aabbMin.y, minY);
      minZ = std::min(aabbMin.z, minZ);

      maxX = std::max(aabbMax.x, maxX);
      maxY = std::max(aabbMax.y, maxY);
      maxZ = std::max(aabbMax.z, maxZ);
    }

  aabb = AABB{minX, minY, minZ, maxX, maxY, maxZ};
}

bool Model::performCulling(
    const std::array<glm::vec4, 6>& frustumPlanes, const glm::mat4& modelMatrix,
    const glm::vec3& cameraPos) {
  return aabb.intersects(frustumPlanes, modelMatrix);
}
#include "model.h"

#include <filesystem>
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
}

Model::Model(TextureManager& textureManager, const Model& borrowTarget,
             const std::filesystem::path& path)
    : handle{borrowTarget.handle},
      texManager{textureManager},
      textureSet{textureManager.borrowTextureSet(borrowTarget.handle)} {
  loadModel(path, true);
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
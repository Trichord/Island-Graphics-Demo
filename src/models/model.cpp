#include "model.h"

#include <glad/glad.h>

#include <fastgltf/types.hpp>
#include <filesystem>
#include <vector>

#include "fastgltf_loader.h"
#include "image_loader.h"
#include "primitive.h"
#include "texture_utilities.h"
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
    : meshes{std::move(other.meshes)},
      handle{other.handle},
      texManager{other.texManager},
      textureSet{other.textureSet} {
  other.handle = 0;
}

Model::~Model() {
  if (handle) texManager.releaseModel(handle);
}

void Model::drawPrimitive(const Primitive& primitive) {
  glDrawElements(GL_TRIANGLES, primitive.getIndicesSize(), GL_UNSIGNED_INT, 0);
}

void Model::render() {
  int lastMaterialIndex{-1};
  bool alphaEnabled{false};
  bool needsAlpha{false};

  for (const Primitive::Mesh& mesh : meshes) {
    for (const Primitive& primitive : mesh.primitives) {
      int materialIndex{primitive.getMaterialIndex()};
      needsAlpha = primitive.getAlphaMode() != fastgltf::AlphaMode::Opaque;

      // Switch materials
      if (materialIndex != lastMaterialIndex && materialIndex != -1 &&
          textureSet.size()) {
        lastMaterialIndex = materialIndex;

        const TextureSet& textures{textureSet[materialIndex]};

        if (textures.albedo) glBindTextureUnit(0, textures.albedo);

        if (textures.occlusionRoughnessMetallic)
          glBindTextureUnit(1, textures.occlusionRoughnessMetallic);

        if (textures.normal) glBindTextureUnit(2, textures.normal);

        if (textures.cubemap) glBindTextureUnit(3, textures.cubemap);
      }

      // Per-primitive alpha mode
      if (needsAlpha != alphaEnabled) {
        if (needsAlpha) {
          glDisable(GL_CULL_FACE);
          glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        } else {
          glEnable(GL_CULL_FACE);
          glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        }
        alphaEnabled = needsAlpha;
      }

      glBindVertexArray(primitive.getVAO());
      drawPrimitive(primitive);
    }
  }

  if (alphaEnabled) {
    glEnable(GL_CULL_FACE);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
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
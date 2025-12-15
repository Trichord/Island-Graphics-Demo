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

void Model::render() {
  for (const Primitive::Mesh& mesh : meshes) {
    for (const Primitive& primitive : mesh.primitives) {
      int materialIndex{primitive.getMaterialIndex()};

      if (materialIndex != -1 && textureSet.size()) {
        const TextureSet& textures{textureSet[materialIndex]};

        if (textures.albedo) {
          glActiveTexture(GL_TEXTURE0);
          glBindTexture(GL_TEXTURE_2D, textures.albedo);
        }

        if (textures.occlusionRoughnessMetallic) {
          glActiveTexture(GL_TEXTURE1);
          glBindTexture(GL_TEXTURE_2D, textures.occlusionRoughnessMetallic);
        }

        if (textures.normal) {
          glActiveTexture(GL_TEXTURE2);
          glBindTexture(GL_TEXTURE_2D, textures.normal);
        }
      }

      glBindVertexArray(primitive.getVAO());
      glDrawElements(GL_TRIANGLES, primitive.getIndicesSize(), GL_UNSIGNED_INT,
                     0);
    }
  }

  glBindVertexArray(0);
  glActiveTexture(GL_TEXTURE0);
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
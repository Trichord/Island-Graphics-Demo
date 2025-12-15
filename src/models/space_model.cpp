#include "space_model.h"

#include <fastgltf/types.hpp>
#include <filesystem>
#include <vector>

#include "fastgltf_loader.h"
#include "glad/glad.h"
#include "primitive.h"
#include "texture_manager.h"

SpaceModel::SpaceModel(TextureManager& textureManager,
                       const std::filesystem::path& modelPath,
                       const std::vector<std::filesystem::path>& imagePaths)
    : CubeMappedModel{textureManager} {
  loadModel(modelPath);
  textureManager.queueCubemapTextureSet(handle, imagePaths, "./", false);
}

SpaceModel::SpaceModel(TextureManager& textureManager,
                       const SpaceModel& borrowTarget,
                       const std::filesystem::path& modelPath,
                       const std::vector<std::filesystem::path>& imagePaths)
    : CubeMappedModel{textureManager, borrowTarget} {
  loadModel(modelPath);
  textureManager.queueCubemapTextureSet(handle, imagePaths, "./", false);
}

SpaceModel::SpaceModel(SpaceModel&& other) noexcept
    : CubeMappedModel{std::move(other)} {
  meshes = std::move(other.meshes);
}

void SpaceModel::render() {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, textureSet[0].cubemap);

  for (const Primitive::Mesh& mesh : meshes)
    for (const Primitive& primitive : mesh.primitives) {
      glBindVertexArray(primitive.getVAO());
      glDrawElements(GL_TRIANGLES, primitive.getIndicesSize(), GL_UNSIGNED_INT,
                     0);
    }

  glBindVertexArray(0);
  glActiveTexture(GL_TEXTURE0);
}

void SpaceModel::loadModel(const std::filesystem::path& path) {
  fastgltf::Asset asset{fastgltfLoader::loadAsset(path)};
  meshes.resize(asset.meshes.size());
  meshes = fastgltfLoader::loadMeshes(asset);
}
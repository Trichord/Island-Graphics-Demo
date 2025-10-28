#include "space_model.h"

#include <fastgltf/types.hpp>
#include <filesystem>
#include <vector>

#include "fastgltf_loader.h"
#include "primitive.h"
#include "shader.h"
#include "glad/glad.h"

SpaceModel::SpaceModel(const std::filesystem::path& modelPath,
                       const std::vector<std::filesystem::path>& imagePaths)
    : CubeMappedModel{imagePaths} {
  loadModel(modelPath);
}

SpaceModel::SpaceModel(SpaceModel&& other) noexcept
    : CubeMappedModel{std::move(other)} {
  meshes = std::move(other.meshes);
}

SpaceModel& SpaceModel::operator=(SpaceModel&& other) noexcept {
  if (this != &other) {
    CubeMappedModel::operator=(std::move(other));
    meshes = std::move(other.meshes);
  }
  return *this;
}

void SpaceModel::render() {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTextureID);

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
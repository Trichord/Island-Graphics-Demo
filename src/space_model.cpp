#include "../headers/space_model.h"

#include <fastgltf/types.hpp>
#include <filesystem>
#include <vector>

#include "../headers/fastgltf_loader.h"
#include "../headers/primitive.h"
#include "../headers/shader.h"
#include "glad/glad.h"

SpaceModel::SpaceModel(const std::filesystem::path& path) {
  texturePaths = {"./Textures/nebula/right.png", "./Textures/nebula/left.png",
                  "./Textures/nebula/top.png",   "./Textures/nebula/bottom.png",
                  "./Textures/nebula/front.png", "./Textures/nebula/back.png"};

  loadTextures();
  loadModel(path);
}

void SpaceModel::render() {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTextureID);

  for (const Primitive::Mesh& mesh : meshes)
    for (const Primitive& primitive : mesh.primitives) primitive.draw();

  glBindVertexArray(0);
  glActiveTexture(GL_TEXTURE0);
}

void SpaceModel::loadModel(const std::filesystem::path& path) {
  fastgltf::Asset asset = fastgltfLoader::loadAsset(path);
  meshes.resize(asset.meshes.size());
  meshes = fastgltfLoader::loadMeshes(asset);
}
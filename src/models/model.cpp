#include "model.h"

#include <glad/glad.h>

#include <fastgltf/types.hpp>
#include <filesystem>
#include <vector>

#include "fastgltf_loader.h"
#include "image_loader.h"
#include "primitive.h"
#include "texture_utilities.h"

Model::Model(const std::filesystem::path& path) { loadModel(path); }

Model::Model(Model&& other) noexcept
    : asset{std::move(other.asset)},
      meshes{std::move(other.meshes)},
      materials{std::move(other.materials)},
      assetImages{std::move(other.assetImages)} {
  other.assetImages.clear();
}

Model& Model::operator=(Model&& other) noexcept {
  if (this != &other) {
    deleteTextureIDs();

    asset = std::move(other.asset);
    meshes = std::move(other.meshes);
    materials = std::move(other.materials);
    assetImages = std::move(other.assetImages);

    other.asset = fastgltf::Asset{};
    other.assetImages.clear();
  }

  return *this;
}

Model::~Model() { deleteTextureIDs(); }

void Model::render() {
  for (const Primitive::Mesh& mesh : meshes) {
    for (const Primitive& primitive : mesh.primitives) {
      int materialIndex{primitive.getMaterialIndex()};
      int albedoIndex{-1};
      int aoMetRoughIdx{-1};
      int normIndex{-1};

      if (materialIndex != -1 && materials.size()) {
        Material material{materials[materialIndex]};

        albedoIndex = material.albedoImageIndex;
        aoMetRoughIdx = material.aoMetallicRoughnessImageIndex;
        normIndex = material.normalImageIndex;
      }

      if (albedoIndex != -1) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, assetImages[albedoIndex]);
      }

      if (aoMetRoughIdx != -1) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, assetImages[aoMetRoughIdx]);
      }

      if (normIndex != -1) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, assetImages[normIndex]);
      }

      glBindVertexArray(primitive.getVAO());
      glDrawElements(GL_TRIANGLES, primitive.getIndicesSize(), GL_UNSIGNED_INT,
                     0);
    }
  }

  glBindVertexArray(0);
  glActiveTexture(GL_TEXTURE0);
}

void Model::loadModel(const std::filesystem::path& path) {
  asset = fastgltfLoader::loadAsset(path);

  meshes.resize(asset.meshes.size());
  meshes = fastgltfLoader::loadMeshes(asset);

  assetImages.resize(asset.images.size());
  loadTextures(path.parent_path());

  asset = fastgltf::Asset{};
}

void Model::loadTextures(const std::filesystem::path& parentPath) {
  // Get all local Uniform Resource Identifiers from gltf, so we can pass the
  // file paths to the image loader
  std::vector<std::filesystem::path> texturePaths{
      fastgltfLoader::getURIs(asset, parentPath)};

  // Load images
  ImageLoader imageLoader{texturePaths};
  std::vector<ImageData> imageData{imageLoader.getImageData()};

  // Pass the image data so that OpenGL texture IDs can be generated and
  // attached to the appropriate materials
  loadMaterials(imageData);
}

void Model::loadMaterials(const std::vector<ImageData>& imageData) {
  materials.resize(asset.materials.size());

  auto processMaterialTexture = [&](const fastgltf::TextureInfo& textureInfo,
                                    bool sRGB) {
    auto [index, sampler] = fastgltfLoader::getTextureInfo(asset, textureInfo);

    // Generate new texture id if the index is not already in the set image
    // indices already processed
    if (processedImageIndices.insert(index).second) {
      GLuint texID{
          textureUtilities::generateTextureID(imageData[index], sampler, sRGB)};
      assetImages[index] = texID;
    }

    return static_cast<unsigned int>(index);
  };

  // Process all materials in the gltf asset
  for (unsigned int i = 0; i < asset.materials.size(); ++i) {
    const auto& material = asset.materials[i];

    if (material.pbrData.baseColorTexture.has_value()) {
      auto& baseColorTexture = material.pbrData.baseColorTexture.value();
      materials[i].albedoImageIndex =
          processMaterialTexture(baseColorTexture, true);
    }

    if (material.pbrData.metallicRoughnessTexture.has_value()) {
      auto& metallicRoughnessTexture =
          material.pbrData.metallicRoughnessTexture.value();
      materials[i].aoMetallicRoughnessImageIndex =
          processMaterialTexture(metallicRoughnessTexture, false);
    }

    if (material.normalTexture.has_value()) {
      auto& normalTexture = material.normalTexture.value();
      materials[i].normalImageIndex =
          processMaterialTexture(normalTexture, false);
    }
  }
}

void Model::deleteTextureIDs() {
  for (const GLuint& textureID : assetImages) glDeleteTextures(1, &textureID);
}
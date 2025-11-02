#pragma once

#include <glad/glad.h>

#include <fastgltf/types.hpp>
#include <filesystem>
#include <unordered_set>
#include <vector>

#include "image_loader.h"
#include "primitive.h"
#include "renderable.h"

/* Model owns the OpenGL Texture IDs and copying is disabled to avoid double
 * frees. Texture IDs are released on Model destruction. assetImages is a 1:1
 * mapping of gltf's images array, but with OpenGL texture IDs instead of URIs.
 * Order of fastgltf's images is preserved */

class Model : public Renderable {
 public:
  Model() = default;
  explicit Model(const std::filesystem::path& path);
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;
  Model(Model&& other) noexcept;
  Model& operator=(Model&& other) noexcept;
  ~Model();

  void render() override;

 protected:
  struct Material {
    int albedoImageIndex, aoMetallicRoughnessImageIndex, normalImageIndex;

    Material(int alb = -1, int metRough = -1, int norm = -1, int ao = -1)
        : albedoImageIndex(alb),
          aoMetallicRoughnessImageIndex(metRough),
          normalImageIndex(norm) {};
  };

  fastgltf::Asset asset;
  std::vector<Primitive::Mesh> meshes;
  std::vector<Material> materials;
  std::vector<GLuint> assetImages;
  std::unordered_set<unsigned int> processedImageIndices;

  void loadModel(const std::filesystem::path& path);
  void loadTextures(const std::filesystem::path& parentPath);
  void loadMaterials(const std::vector<ImageData>& imagedata);
  void deleteTextureIDs();
};

#pragma once

#include <glad/glad.h>

#include <fastgltf/types.hpp>
#include <filesystem>
#include <unordered_set>
#include <vector>

#include "image_loader.h"
#include "primitive.h"
#include "renderable.h"

/**
 * @brief A model loaded from a glTF file.
 *
 * @details Model owns the OpenGL texture ID's and copying is disabled to avoid
 * double frees.  Texture IDs are released upon destruction.  The member
 * assetImages is a 1:1 mapping of glTF's images array, but with OpenGL texture
 * IDs instead of URIs. Order between fastgltf's image indices and assetImages
 * is preserved.
 */
class Model : public Renderable {
 public:
  Model() = default;
  explicit Model(const std::filesystem::path& path);
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;
  Model(Model&& other) noexcept;
  Model& operator=(Model&& other) noexcept;
  ~Model();

  /**
   * @brief Model class's implementation of the Renderable interface.
   *
   * @details Draws each primitive in the mesh. Binds albdeo, ambient occlusion,
   *  metallic, roughness, and normal map textures, if the model has them.
   */
  void render() override;

 protected:
  struct Material {
    int albedoImageIndex, aoMetallicRoughnessImageIndex, normalImageIndex;

    Material(int alb = -1, int metRough = -1, int norm = -1, int ao = -1)
        : albedoImageIndex(alb),
          aoMetallicRoughnessImageIndex(metRough),
          normalImageIndex(norm) {};
  };

  // fastgltf::Asset asset;
  std::vector<Primitive::Mesh> meshes;
  std::vector<Material> materials;
  std::vector<GLuint> assetImages;
  std::unordered_set<unsigned int> processedImageIndices;

  /**
   * @brief Calls fastgltf to load a model.
   *
   * @param path The path to the .gltf file.
   */
  void loadModel(const std::filesystem::path& path);

  /**
   * @brief Uses fastgltf to retrieve all texture image URI's. Turns the URI's
   * into image data.
   *
   * @param parentPath The folder containing the .gltf file.
   * @return A vector of image data after being processed by stb_image.
   */
  void loadTextureImages(fastgltf::Asset& asset,
                         const std::filesystem::path& parentPath);
  void loadMaterials(fastgltf::Asset& asset,
                     const std::vector<LoadedImage>& imagedata);
  void deleteTextureIDs();
};

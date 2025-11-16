#pragma once

#include <glad/glad.h>

#include <fastgltf/types.hpp>
#include <filesystem>
#include <unordered_set>
#include <vector>

#include "image_loader.h"
#include "primitive.h"
#include "renderable.h"
#include "texture_manager.h"


class Model : public Renderable {
 public:
  Model(TextureManager& textureManager,
        const std::filesystem::path& path);
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;
  Model(Model&& other) noexcept;
  Model& operator=(Model&& other) = delete;
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
  
  ModelHandle handle{0};
  TextureManager& texManager;
  const std::vector<TextureSet>& textureSet;
  std::vector<Primitive::Mesh> meshes;

  void loadModel(const std::filesystem::path& path);

};

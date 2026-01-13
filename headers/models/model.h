#pragma once

#include <filesystem>
#include <vector>

#include "aabb.h"
#include "primitive.h"
#include "renderable.h"
#include "texture_manager.h"

class Model : public Renderable {
 public:
  Model(TextureManager& textureManager, const std::filesystem::path& path);
  Model(TextureManager& textureManager, const Model& borrowTarget,
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
  void render(unsigned int globalMSAA) override;
  bool performCulling(const std::array<glm::vec4, 6>& frustumPlanes,
                      const glm::mat4& modelMatrix,
                      const glm::vec3& cameraPos) override;

  ModelHandle getModelHandle();

 protected:
  struct MaterialIndicies {
    int albedoImageIndex, aoMetallicRoughnessImageIndex, normalImageIndex;

    MaterialIndicies(int alb = -1, int metRough = -1, int norm = -1,
                     int ao = -1)
        : albedoImageIndex(alb),
          aoMetallicRoughnessImageIndex(metRough),
          normalImageIndex(norm) {};
  };

  ModelHandle handle{0};
  TextureManager& texManager;
  const std::vector<Material>& textureSet;
  std::vector<Primitive::Mesh> meshes;
  AABB aabb;

  void loadModel(const std::filesystem::path& path, bool borrowed);
  void bindTextures(const Material& textureSet);
  void calculateAABB();
  void setAlphaMode(unsigned int globalMSAA, bool needsAlpha);
  virtual void drawPrimitive(const Primitive& primitive);
  void assignMaterialIndex(std::size_t newIndex);
  bool hasMaterials() const;
};

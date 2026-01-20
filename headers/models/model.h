#pragma once

#include <array>
#include <filesystem>
#include <vector>

#include "aabb.h"
#include "glm/glm.hpp"
#include "primitive.h"
#include "renderable.h"
#include "texture_manager.h"
#include "gl_state.h"

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
  void render(GLState& glState) override;
  bool performCulling(const std::array<glm::vec4, 6>& frustumPlanes,
                      const glm::mat4& modelMatrix, const glm::vec3& cameraPos,
                      const float cullDistanceScale) override;

  ModelHandle getModelHandle();

 protected:
     enum MaterialSlots {
         ALBEDO,
         ARM,
         NORMAL,
         CUBEMAP
  };
  ModelHandle handle{0};
  TextureManager& texManager;
  const std::vector<Material>& materials;
  std::vector<Primitive::Mesh> meshes;
  AABB aabb;

  void loadModel(const std::filesystem::path& path, bool borrowed);
  void calculateAABB();
  void bindTextures(GLState& glState, const Material& materials);
  virtual void drawPrimitive(const Primitive& primitive);
  void assignMaterialIndex(std::size_t newIndex);
  bool hasMaterials() const;
};

#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include "aabb.h"
#include "instance_data.h"
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

  AABB& getAABB() override;
  void resetVisibility() override;
  std::size_t getInstanceCount() const override;
  std::span<Primitive::Mesh> getMeshes() override;
  const ModelHandle getModelHandle() const override;
  std::span<InstanceData> getInstanceData() override;
  void setVisibleInstanceCount(std::size_t newSize) override;

 protected:
  ModelHandle handle{0};
  TextureManager& texManager;
  std::vector<Primitive::Mesh> meshes;
  AABB aabb;

  void loadModel(const std::filesystem::path& path, bool borrowed);
  void calculateAABB();
  void assignMaterialIndex(std::size_t newIndex);
  bool hasMaterials() const;
};

#pragma once

#include <filesystem>
#include <vector>

#include "cube_mapped_model.h"
#include "primitive.h"
#include "texture_manager.h"

class SpaceModel : public CubeMappedModel {
 public:
  SpaceModel(TextureManager& textureManager,
             const std::filesystem::path& modelPath,
             const std::vector<std::filesystem::path>& imagePaths);
  SpaceModel(SpaceModel&& other) noexcept;
  SpaceModel& operator=(SpaceModel&& other) = delete;
  ~SpaceModel() = default;

  void render() override;

 private:
  std::vector<Primitive::Mesh> meshes;
  void loadModel(const std::filesystem::path& path);
};
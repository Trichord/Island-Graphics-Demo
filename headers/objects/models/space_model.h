#pragma once

#include <filesystem>
#include <vector>

#include "cube_mapped_model.h"
#include "primitive.h"

class SpaceModel : public CubeMappedModel {
 public:
  SpaceModel(const std::filesystem::path& modelPath,
             const std::vector<std::filesystem::path>& imagePaths);
  SpaceModel(SpaceModel&& other) noexcept;
  SpaceModel& operator=(SpaceModel&& other) noexcept;
  ~SpaceModel() = default;

  void render() override;

 protected:
  std::vector<Primitive::Mesh> meshes;
  void loadModel(const std::filesystem::path& path);
};
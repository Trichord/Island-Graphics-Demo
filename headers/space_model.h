#pragma once

#include <filesystem>
#include <vector>

#include "cube_mapped_model.h"
#include "primitive.h"
#include "shader.h"

class SpaceModel : public CubeMappedModel {
 public:
  SpaceModel(std::filesystem::path path);
  void render(Shader& shader) override;

 protected:
  std::vector<Primitive::Mesh> meshes;
  void loadModel(std::filesystem::path& path);
};
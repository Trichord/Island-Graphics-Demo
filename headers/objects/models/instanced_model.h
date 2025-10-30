#pragma once

#include <glad/glad.h>

#include <filesystem>
#include <glm/glm.hpp>

#include "model.h"

class InstancedModel : public Model {
 public:
  InstancedModel(const std::filesystem::path& modelPath,
                 const std::filesystem::path& positionsPath,
                 unsigned int numPositions = 0);
  InstancedModel(const InstancedModel&) = delete;
  InstancedModel& operator=(const InstancedModel&) = delete;
  InstancedModel(InstancedModel&& other) noexcept;
  InstancedModel& operator=(InstancedModel&& other) noexcept;
  ~InstancedModel();

 private:
  GLuint transformsVBO{0};
  GLuint positionsVBO{0};
  std::vector<glm::mat4> transforms;
  std::vector<glm::vec3> positions;

  void render() override;
  void loadPositions(const std::filesystem::path& positionsPath,
                     unsigned int numPositions);
  void setupInstancedBuffers();
  void generateTransforms();
  void deleteBuffers();
};

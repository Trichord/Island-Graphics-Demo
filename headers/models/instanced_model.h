#pragma once

#include <filesystem>

#include "glad/glad.h"
#include "glm/glm.hpp"
#include "model.h"
#include "texture_manager.h"

class InstancedModel : public Model {
 public:
  InstancedModel(TextureManager& textureManager,
                 const std::filesystem::path& modelPath,
                 const std::filesystem::path& positionsPath,
                 float minSize = 1.0f, float maxSize = 1.0f,
                 unsigned int numPositions = 0);
  InstancedModel(TextureManager& textureManager,
                 const InstancedModel& borrowTarget,
                 const std::filesystem::path& modelPath,
                 const std::filesystem::path& positionsPath,
                 float minSize = 1.0f, float maxSize = 1.0f,
                 unsigned int numPositions = 0);
  InstancedModel(const InstancedModel&) = delete;
  InstancedModel& operator=(const InstancedModel&) = delete;
  InstancedModel(InstancedModel&& other) noexcept;
  InstancedModel& operator=(InstancedModel&& other) = delete;
  ~InstancedModel();

  bool performCulling(const std::array<glm::vec4, 6>& frustumPlanes,
                      const glm::mat4& modelMatrix) override;

 private:
  GLuint transformsVBO{0};
  GLuint positionsVBO{0};
  GLuint normalMatricesVBO{0};
  std::vector<glm::mat4> transforms;
  std::vector<glm::vec3> positions;
  std::vector<glm::mat4> normalMatrices;

  void drawPrimitive(const Primitive& primative) override;
  void loadPositions(const std::filesystem::path& positionsPath,
                     unsigned int numPositions);
  void setupInstancedBuffers();
  void generateTransforms(float minSize, float maxSize);
  void deleteBuffers();
};

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
                      const glm::mat4& modelMatrix, const glm::vec3& cameraPos,
                      const float cullDistanceScale) override;

 private:
  struct InstanceData {
    glm::mat4 modelMatrix{1.0f};
    glm::mat4 normalMatrix{1.0f};
    glm::vec3 position{0.0f};
    AABB worldAABB;
    bool isVisible{false};
    double sortKey{0.0f};
  };

  std::vector<InstanceData> instances;
  std::size_t visibleInstances{0};

  GLuint instancesVBO{0};

  void drawPrimitive(const Primitive& primative) override;
  void loadPositions(const std::filesystem::path& positionsPath,
                     unsigned int numPositions);
  void setupInstancedBuffers();
  void generateInstanceMatrices(float minSize, float maxSize);
  void generateWorldAABBs();
  void deleteBuffers();
};

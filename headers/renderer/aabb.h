#pragma once

#include <array>

#include "glm/glm.hpp"

class AABB {
 public:
  AABB() = default;
  AABB(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);
  AABB(const glm::vec3& min_, const glm::vec3& max_);

  bool intersects(const std::array<glm::vec4, 6>& planes, const glm::mat4& modelMatrix) const;
  void resize(float scale);

 private:
  glm::vec3 min{0.0f};
  glm::vec3 max{0.0f};
};
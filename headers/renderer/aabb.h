#pragma once

#include <array>

#include "glm/glm.hpp"

class AABB {
 public:
  AABB() = default;
  AABB(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);
  AABB(const glm::vec3& min_, const glm::vec3& max_);

  void resize(float scale);
  glm::vec3 getCenter() const;
  glm::vec3 getExtent() const;

  // Intersect methods are credit: Fabian “ryg” Giesen
  bool intersects(const std::array<glm::vec4, 6>& planes,
                  const glm::mat4& modelMatrix) const;
  bool intersectsSinglePlane(const glm::vec4& plane,
                             const glm::mat4& modelMatrix) const;

  bool intersectsWorldSpace(const std::array<glm::vec4, 6>& planes) const;
  bool intersectsWorldSinglePlane(const glm::vec4& plane) const;

 private:
  glm::vec3 center{0.0f};
  glm::vec3 extent{0.0f};
};
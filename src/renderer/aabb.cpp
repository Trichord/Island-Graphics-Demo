#include "aabb.h"

#include "glm/glm.hpp"

AABB::AABB(float minX, float minY, float minZ, float maxX, float maxY,
           float maxZ)
    : min{minX, minY, minZ}, max{maxX, maxY, maxZ} {}

AABB::AABB(const glm::vec3& min_, const glm::vec3& max_)
    : min{min_}, max{max_} {}

// Credit: Fabian “ryg” Giesen
bool AABB::intersects(const std::array<glm::vec4, 6>& planes,
                      const glm::mat4& modelMatrix) const {
  glm::vec3 center = 0.5f * (min + max);
  glm::vec3 extent = 0.5f * (max - min);

  glm::mat3 RS{glm::mat3{modelMatrix}};
  glm::mat3 absRS{glm::abs(RS[0]), glm::abs(RS[1]), glm::abs(RS[2])};

  glm::vec3 worldCenter{modelMatrix * glm::vec4(center, 1.0f)};
  glm::vec3 worldExtent{absRS * extent};

  for (const auto& plane : planes) {
    glm::vec3 normal{glm::vec3{plane}};

    if (glm::dot(worldCenter, normal) +
            glm::dot(worldExtent, glm::abs(normal)) <
        -plane.w)
      return false;
  }

  return true;
}

void AABB::resize(float scale) {
  min *= scale;
  max *= scale;
}
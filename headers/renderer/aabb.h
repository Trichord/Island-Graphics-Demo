#pragma once

#include <array>

#include "glm/glm.hpp"

class AABB
{
  public:
    AABB() = default;
    AABB(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);
    AABB(const glm::vec3 &min, const glm::vec3 &max);

    glm::vec3 getMin() const;
    glm::vec3 getMax() const;
    const glm::vec3& getCenter() const;
    const glm::vec3& getExtent() const;

    void resize(float scale);

    // Intersect methods are credit: Fabian “ryg” Giesen
    bool intersects(const std::array<glm::vec4, 6> &planes, const glm::mat4 &modelMatrix) const;
    bool intersects(const std::array<glm::vec4, 6> &planes) const;

    // Returns a new AABB, translated by the model matrix
    AABB transform(const glm::mat4 &modelMatrix) const;

  private:
    glm::vec3 center_{0.0f};
    glm::vec3 extent_{0.0f};
};
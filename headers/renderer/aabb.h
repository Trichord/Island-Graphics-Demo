#pragma once

#include <array>

#include "glm/glm.hpp"

class AABB
{
  public:
    AABB() = default;
    AABB(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);
    AABB(const glm::vec3 &min_, const glm::vec3 &max_);

    float getMinX() const;
    float getMinY() const;
    float getMinZ() const;
    float getMaxX() const;
    float getMaxY() const;
    float getMaxZ() const;
    glm::vec3 getMin() const;
    glm::vec3 getMax() const;

    void resize(float scale);
    glm::vec3 getCenter() const;
    glm::vec3 getExtent() const;

    // Intersect methods are credit: Fabian “ryg” Giesen
    bool intersects(const std::array<glm::vec4, 6> &planes, const glm::mat4 &modelMatrix) const;   
    bool intersectsWorldSpace(const std::array<glm::vec4, 6> &planes) const;

  private:
    glm::vec3 center{0.0f};
    glm::vec3 extent{0.0f};
};
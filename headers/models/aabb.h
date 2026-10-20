#pragma once

#include <array>

#include <glm/glm.hpp>

class AABB
{
  public:
    AABB() = default;
    AABB(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);
    AABB(const glm::vec3 &min, const glm::vec3 &max);

    glm::vec3 getMin() const;
    glm::vec3 getMax() const;
    const glm::vec3 &getCenter() const;
    const glm::vec3 &getHalfExtent() const;

    void setCenter(const glm::vec3 &center);
    void setHalfExtent(const glm::vec3 &extent);
    void resize(float scale);

    bool intersects(const std::array<glm::vec4, 6> &planes, const glm::mat4 &modelMatrix) const;
    bool intersects(const std::array<glm::vec4, 6> &planes) const;

    bool pointInside(const glm::vec3 &point) const;

    // Returns a new AABB, translated by the model matrix
    AABB transform(const glm::mat4 &modelMatrix) const;

  private:
    glm::vec3 center_{0.0f};
    glm::vec3 halfExtent_{0.0f};

    // Intersect method: Fabian "ryg" Giesen
    bool aabbIntersects(const std::array<glm::vec4, 6> &planes, const glm::vec3 &center,
                        const glm::vec3 &halfExtent) const;
};
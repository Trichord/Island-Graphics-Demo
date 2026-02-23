#pragma once

#include <optional>

#include "aabb.h"
#include "glm/glm.hpp"

class Capsule
{
  public:
    struct CollisionResolution
    {
        float depth{0.0f};
        glm::vec3 normal{0.0f};
    };

    Capsule() = default;
    Capsule(const glm::vec3 &position, float height, float radius);

    float getHeight() const;
    glm::vec3 getBottom() const;
    glm::vec3 getPosition() const;
    const glm::vec3 &getVelocity() const;
    void translate(const glm::vec3 &direction);
    void addVelocity(const glm::vec3 &velocity);
    void setPosition(const glm::vec3 &newPosition);
    void setVelocity(const glm::vec3 &newVelocity);
    std::optional<CollisionResolution> collideWithAABB(const AABB &aabb);
    std::optional<CollisionResolution> collideWithCircle(const glm::vec3 &center, float otherRadius);
    CollisionResolution collideWithTriangle(const glm::vec3 &A, const glm::vec3 &B, const glm::vec3 &C);

    // Sliding physics credit: Wicked Engine https://wickedengine.net/2020/04/capsule-collision-detection
    void slide(const glm::vec3 &normal);

  private:
    glm::vec3 top{1.0f};
    glm::vec3 bottom{0.0f};
    glm::vec3 velocity{0.0f};
    float height{1.0f};
    float radius{0.5f};
};
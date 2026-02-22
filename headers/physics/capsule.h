#pragma once

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

    glm::vec3 getBottom() const;
    glm::vec3 getPosition() const;
    void setPosition(const glm::vec3 &newPosition);
    CollisionResolution collision(const glm::vec3 &A, const glm::vec3 &B, const glm::vec3 &C);

  private:
    glm::vec3 top{1.0f};
    glm::vec3 bottom{0.0f};
    float height{1.0f};
    float radius{0.5};
};
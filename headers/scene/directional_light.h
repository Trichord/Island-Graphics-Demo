#pragma once

#include <glm/glm.hpp>

class DirectionalLight
{
  public:
    DirectionalLight() = default;
    DirectionalLight(const glm::vec3 &color, const glm::vec3 &direction);

    const glm::vec3 &getColor() const;
    const glm::vec3 &getDirection() const;

    void setColor(const glm::vec3 &newColor);
    void setDirection(const glm::vec3 &direction);

    bool directionHasUpdated() const;
    bool colorHasUpdated() const;
    void clearFlags();

  private:
    glm::vec3 color_{1.0f};
    glm::vec3 direction_{0.0f};

    mutable bool colorUpdated_{true};
    mutable bool directionUpdated_{true};
};

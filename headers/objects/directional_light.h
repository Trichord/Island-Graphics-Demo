#pragma once

#include "light.h"
#include <glm/glm.hpp>

class DirectionalLight : public Light {
 public:
  DirectionalLight() = default;
  DirectionalLight(const glm::vec3& newDirection,
                            const glm::vec3& newColor = glm::vec3(1.0f));
  const glm::vec3& getColor() const override;
  const glm::vec3& getDirection() const;
  void setDirection(glm::vec3& newDirection);
  bool directionHasUpdated();

 private:
  bool directionUpdated{true};
  glm::vec3 direction{0.0f};
};

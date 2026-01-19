#pragma once

#include "glm/glm.hpp"

#include "light.h"

class DirectionalLight : public Light {
 public:
  DirectionalLight() = default;
  DirectionalLight(const glm::vec3& newDirection,
                   const glm::vec3& newColor = glm::vec3(1.0f));
  glm::vec3 getColor() const override;
  glm::vec3 getDirection() const;
  void setDirection(const glm::vec3& newDirection);
  bool directionHasUpdated();

 private:
  bool directionUpdated{true};
  glm::vec3 direction{0.0f};
};

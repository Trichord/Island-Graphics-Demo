#pragma once

#include <glm/glm.hpp>

#include "light.h"

class PointLight : public Light {
 public:
  PointLight() = default;
  PointLight(const glm::vec3& newPosition,
             const glm::vec3& newColor = glm::vec3(1.0f));
  const glm::vec3& getColor() const override;
  const glm::vec3& getPosition() const;
  void setPosition(const glm::vec3& newPosition);
  bool positionHasUpdated();

 private:
  bool positionUpdated{true};
  glm::vec3 position{0.0f};
};
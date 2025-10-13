#pragma once

#include "light.h"
#include <glm/glm.hpp>

class PointLight : public Light {
 public:
  PointLight() = default;
  PointLight(const glm::vec3& newPosition);
  const glm::vec3& getColor() const override;
  const glm::vec3& getPosition() const;
  void setPosition(glm::vec3& newPosition);
  bool positionHasUpdated();

 private:
  bool positionUpdated = true;
  glm::vec3 position = glm::vec3(0.0f);
};
#include "../headers/directional_light.h"
#include <glm/glm.hpp>

DirectionalLight::DirectionalLight(const glm::vec3& newDirection,
                                   const glm::vec3& newColor)
    : direction(newDirection), Light(newColor) {}

const glm::vec3& DirectionalLight::getColor() const { return color; }

const glm::vec3& DirectionalLight::getDirection() const { return direction; }

void DirectionalLight::setDirection(glm::vec3& newDirection) {
  direction = newDirection;
  directionUpdated = true;
}

bool DirectionalLight::directionHasUpdated() {
  if (directionUpdated) {
    directionUpdated = false;
    return true;
  } else
    return false;
}
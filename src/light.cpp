#include "../headers/light.h"
#include <glm/glm.hpp>

Light::Light(const glm::vec3& newColor) : color{newColor} {}

void Light::setColor(const glm::vec3& newColor) {
  color = newColor;
  colorUpdated = true;
}

bool Light::colorHasUpdated() {
  if (colorUpdated) {
    colorUpdated = false;
    return true;
  } else
    return false;
}
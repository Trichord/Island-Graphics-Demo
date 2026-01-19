#pragma once

#include "glm/glm.hpp"

/* Abstract class light.  getColor was chosen as the abstract hook due to its
 * ease of implementation. */

class Light {
 public:
  explicit Light(const glm::vec3& newColor = glm::vec3(1.0f));
  virtual ~Light() = default;
  void setColor(const glm::vec3& newColor);
  virtual glm::vec3 getColor() const = 0;
  bool colorHasUpdated();

 protected:
  glm::vec3 color{1.0f};
  bool colorUpdated{true};
};
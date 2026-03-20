#pragma once

#include "glm/glm.hpp"

/* Abstract class light.  getColor was chosen as the abstract hook due to its
 * ease of implementation. */

class Light
{
  public:
    explicit Light(const glm::vec3 &newColor = glm::vec3(1.0f));
    virtual ~Light() = default;
    void setColor(const glm::vec3 &newColor);
    virtual glm::vec3 getColor() const = 0;
    bool colorHasUpdated();
    const glm::mat4 &getProjectionMatrix() const;
    const glm::mat4 &getViewProjectionMatrix() const;
    void setProjectionMatrix(float left, float right, float bottom, float top, float near, float far);
    void setViewProjectionMatrix(const glm::vec3 &cameraPosition, const glm::vec3 &target, const glm::vec3 &worldUp);

  protected:
    glm::vec3 color_{1.0f};
    glm::mat4 projectionMatrix_{1.0f};
    glm::mat4 viewProjectionMatrix_{1.0f};
    bool colorUpdated_{true};
};
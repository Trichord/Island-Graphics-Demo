#pragma once

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

/* The 'Set absolute' functions will reset that object property then adjust.
 * 'Set relative' will adjust the current state by the amount in the parameter.
 * Rotation is given in degrees, for an x, y, and z. */

class Transform
{
  public:
    Transform(glm::vec3 position = glm::vec3(0.0f), glm::vec3 scale = glm::vec3(1.0f), float degrees = 0.0f,
              glm::vec3 axis = glm::vec3(0.0f, 1.0f, 0.0f));

    void setAbsoluteScale(float absoluteScale);
    void setAbsolutePosition(glm::vec3 absolutePosition);
    void setAbsoluteRotation(glm::vec3 absoluteRotation);

    void setRelativeScale(float relativeScale);
    void setRelativePosition(glm::vec3 relativePosition);
    void setRelativeRotation(glm::vec3 relativeRotation);

    void rotateAroundAxis(float degrees, glm::vec3 axis);

    glm::mat4 &getModelMatrix();
    glm::mat3 &getNormalMatrix();
    glm::vec3 getRotation() const;
    const glm::vec3 &getScale() const;
    const glm::vec3 &getPosition() const;

  private:
    glm::vec3 position{1.0f};
    glm::vec3 scale{1.0f};
    glm::mat4 modelMatrix{1.0f};
    glm::mat3 normalMatrix{1.0f};
    glm::quat quaternion{1.0f, 0.0f, 0.0f, 0.0f};

    void rebuildModelMatrix();
    bool isDirty{true};
};
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

/* The 'Set absolute' functions will reset that object property then adjust.
 * 'Set relative' will adjust the current state by the amount in the parameter.
 * Rotation is given in degrees, for an x, y, and z. */

class Transform
{
  public:
    Transform(glm::vec3 position, glm::vec3 scale, float degrees, glm::vec3 axis = glm::vec3{0.0f, 1.0f, 0.0f});
    Transform(glm::vec3 position = glm::vec3{0.0f}, glm::vec3 scale = glm::vec3{1.0f}, glm::quat quat = {});

    void setAbsoluteScale(float absoluteScale);
    void setAbsoluteTranslation(glm::vec3 absoluteTranslation);
    void setAbsoluteRotation(glm::vec3 absoluteRotation);
    void setAbsoluteRotation(glm::quat quat);

    void setRelativeScale(float relativeScale);
    void setRelativeTranslation(glm::vec3 relativeTranslation);
    void setRelativeRotation(glm::vec3 relativeRotation);

    void rotateAroundAxisLocal(float degrees, glm::vec3 axis);
    void rotateAroundAxisWorld(float degrees, glm::vec3 axis);

    glm::mat4 &getModelMatrix();
    glm::mat3 &getNormalMatrix();
    glm::vec3 getRotation() const;
    const glm::vec3 &getScale() const;
    const glm::vec3 &getPosition() const;

  private:
    glm::vec3 translation_{1.0f};
    glm::vec3 scale_{1.0f};
    glm::mat4 modelMatrix_{1.0f};
    glm::mat3 normalMatrix_{1.0f};
    glm::quat quaternion_{1.0f, 0.0f, 0.0f, 0.0f};

    bool isDirty_{true};

    void rebuildModelMatrix();
};
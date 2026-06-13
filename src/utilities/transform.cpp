#include "utilities/transform.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

Transform::Transform(glm::vec3 translation, glm::vec3 scale, float degrees, glm::vec3 axis)
    : translation_{translation}, scale_{scale}
{
    glm::vec3 normalizedAxis{glm::normalize(axis)};
    quaternion_ = glm::angleAxis(glm::radians(degrees), normalizedAxis);
    rebuildModelMatrix();
    isDirty_ = false;
};

Transform::Transform(glm::vec3 translation, glm::vec3 scale, glm::quat quat)
    : translation_{translation}, scale_{scale}, quaternion_{quat}
{
    rebuildModelMatrix();
    isDirty_ = false;
}

void Transform::setAbsoluteTranslation(glm::vec3 absoluteTranslation)
{
    translation_ = absoluteTranslation;
    isDirty_ = true;
}

void Transform::setAbsoluteRotation(glm::vec3 absoluteRotation)
{
    quaternion_ = glm::quat(glm::radians(absoluteRotation));
    isDirty_ = true;
}

void Transform::setAbsoluteRotation(glm::quat quat)
{
    quaternion_ = quat;
    isDirty_ = true;
}

void Transform::setAbsoluteScale(float absoluteScale)
{
    scale_ = glm::vec3(absoluteScale);
    isDirty_ = true;
}

void Transform::setRelativeTranslation(glm::vec3 relativeTranslation)
{
    translation_ += relativeTranslation;
    isDirty_ = true;
}

void Transform::setRelativeRotation(glm::vec3 relativeRotation)
{
    glm::quat updatedRotation{quaternion_ * glm::quat(glm::radians(relativeRotation))};
    quaternion_ = glm::normalize(updatedRotation);
    isDirty_ = true;
}

void Transform::setRelativeScale(float relativeScale)
{
    scale_ *= relativeScale;
    isDirty_ = true;
}

void Transform::rotateAroundAxisLocal(float degrees, glm::vec3 axis)
{
    glm::vec3 normalizedAxis{glm::normalize(axis)};
    glm::quat updatedRotation{quaternion_ * glm::angleAxis(glm::radians(degrees), normalizedAxis)};
    quaternion_ = glm::normalize(updatedRotation);
    isDirty_ = true;
}

void Transform::rotateAroundAxisWorld(float degrees, glm::vec3 axis)
{
    glm::vec3 normalizedAxis{glm::normalize(axis)};
    glm::quat updatedRotation{glm::angleAxis(glm::radians(degrees), normalizedAxis) * quaternion_};
    quaternion_ = glm::normalize(updatedRotation);
    isDirty_ = true;
}

const glm::vec3 &Transform::getPosition() const
{
    return translation_;
}

const glm::vec3 &Transform::getScale() const
{
    return scale_;
}

glm::mat4 &Transform::getModelMatrix()
{
    if (isDirty_)
    {
        rebuildModelMatrix();
        isDirty_ = false;
    }

    return modelMatrix_;
}

glm::mat3 &Transform::getNormalMatrix()
{
    if (isDirty_)
    {
        rebuildModelMatrix();
        isDirty_ = false;
    }

    return normalMatrix_;
}

glm::vec3 Transform::getRotation() const
{
    return glm::degrees(glm::eulerAngles(quaternion_));
}

void Transform::rebuildModelMatrix()
{
    glm::mat4 newModelMatrix{1.0f};
    newModelMatrix = glm::translate(newModelMatrix, translation_);
    newModelMatrix *= glm::mat4_cast(quaternion_);
    newModelMatrix = glm::scale(newModelMatrix, scale_);

    modelMatrix_ = newModelMatrix;
    normalMatrix_ = glm::transpose(glm::inverse(glm::mat3(newModelMatrix)));
}
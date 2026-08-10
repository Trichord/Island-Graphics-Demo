#include "utilities/transform.h"

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/compatibility.hpp>

#include "utilities/world_vectors.h"

Transform::Transform(glm::vec3 translation, glm::vec3 scale, float degrees, glm::vec3 axis)
    : translation_{translation}, scale_{scale}
{
    glm::vec3 normalizedAxis{glm::normalize(axis)};
    quaternion_ = glm::angleAxis(glm::radians(degrees), normalizedAxis);
    rebuildModelMatrix();
    isDirty_ = false;
};

Transform::Transform(glm::vec3 translation, glm::vec3 scale, glm::quat quat)
    : translation_{translation}, quaternion_{quat}, scale_{scale}
{
    rebuildModelMatrix();
    isDirty_ = false;
}

void Transform::setAbsoluteTranslation(glm::vec3 absoluteTranslation)
{
    translation_ = absoluteTranslation;
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

void Transform::setAbsoluteScale(const glm::vec3 &absoluteScale)
{
    scale_ = glm::vec3(absoluteScale);
    isDirty_ = true;
}

void Transform::setRelativeTranslation(glm::vec3 relativeTranslation)
{
    translation_ += relativeTranslation;
    isDirty_ = true;
}

void Transform::setRelativeScale(float relativeScale)
{
    scale_ *= relativeScale;
    isDirty_ = true;
}

void Transform::setRelativeScale(const glm::vec3 &relativeScale)
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

const glm::quat &Transform::getRotation() const
{
    return quaternion_;
}

const glm::vec3 &Transform::getScale() const
{
    return scale_;
}

glm::vec3 Transform::getFront() const
{
    return quaternion_ * WorldVectors::worldFront;
}

glm::vec3 Transform::getUp() const
{
    return quaternion_ * WorldVectors::worldUp;
}

glm::vec3 Transform::getRight() const
{
    return quaternion_ * WorldVectors::worldRight;
}

float Transform::getYaw() const
{
    glm::vec3 forward{getFront()};
    return glm::degrees(glm::atan2(forward.x, -forward.z));
}

float Transform::getPitch() const
{
    glm::vec3 forward{getFront()};
    return glm::degrees(glm::asin(glm::clamp(forward.y, -1.0f, 1.0f)));
}

const glm::mat4 &Transform::getModelMatrix() const
{
    if (isDirty_)
    {
        rebuildModelMatrix();
        isDirty_ = false;
    }

    return modelMatrix_;
}

const glm::mat3 &Transform::getNormalMatrix() const
{
    if (isDirty_)
    {
        rebuildModelMatrix();
        isDirty_ = false;
    }

    return normalMatrix_;
}

void Transform::rebuildModelMatrix() const
{
    glm::mat4 newModelMatrix{1.0f};
    newModelMatrix = glm::translate(newModelMatrix, translation_);
    newModelMatrix *= glm::mat4_cast(quaternion_);
    newModelMatrix = glm::scale(newModelMatrix, scale_);

    modelMatrix_ = newModelMatrix;
    normalMatrix_ = glm::transpose(glm::inverse(glm::mat3(newModelMatrix)));
}
#include "scene/camera.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "utilities/world_vectors.h"

constexpr float maxPitch{85.0f};
constexpr float minPitch{-maxPitch};
constexpr float sensitivity{0.1f};

Camera::Camera(glm::vec3 position, float yaw, float pitch)
    : yaw_{yaw}, pitch_{pitch}, position_{position},
      projectionMatrix_{glm::perspective(glm::radians(fov_), aspectRatio_, nearPlane_, farPlane_)}
{
}

bool Camera::hasUpdated()
{
    if (hasChanged_)
    {
        hasChanged_ = false;
        return true;
    }
    else
        return false;
}

float Camera::getAspectRatio() const
{
    return aspectRatio_;
}

float Camera::getYaw() const
{
    return yaw_;
}

float Camera::getPitch() const
{
    return pitch_;
}

const glm::vec3 &Camera::getPosition() const
{
    return position_;
}

const glm::mat4 &Camera::getViewMatrix()
{
    if (dirtyMatrices_)
    {
        updateMatrices();
        dirtyMatrices_ = false;
    }

    return viewMatrix_;
}

const glm::mat4 &Camera::getProjectionMatrix()
{
    return projectionMatrix_;
}

const glm::mat4 &Camera::getViewProjectionMatrix()
{
    if (dirtyMatrices_)
    {
        updateMatrices();
        dirtyMatrices_ = false;
    }

    return viewProjectionMatrix_;
}

void Camera::setYaw(float yaw)
{
    yaw_ = std::fmod(yaw, 360.0f);
    hasChanged_ = true;
    dirtyMatrices_ = true;
}

void Camera::setFOV(float fov)
{
    fov_ = fov;
    hasChanged_ = true;
    dirtyMatrices_ = true;
}

void Camera::setPitch(float pitch)
{
    pitch_ = std::clamp(pitch, minPitch, maxPitch);
    hasChanged_ = true;
    dirtyMatrices_ = true;
}

void Camera::setPosition(const glm::vec3 &position)
{
    position_ = position;
    hasChanged_ = true;
    dirtyMatrices_ = true;
}

// Maps world orientation to cubemap directions
void Camera::setWorldOrientation(WorldOrientation orientation)
{
    using enum WorldOrientation;
    using namespace WorldVectors;

    glm::vec3 front{0.0f};
    glm::vec3 up{0.0f};

    switch (orientation)
    {
    case UP:
        front = worldUp;
        up = worldBack;
        break;

    case DOWN:
        front = worldDown;
        up = worldBack;
        break;

    case RIGHT:
        front = worldRight;
        up = worldDown;
        break;

    case LEFT:
        front = worldLeft;
        up = worldDown;
        break;

    case FRONT:
        front = worldFront;
        up = worldDown;
        break;

    case BACK:
        front = worldBack;
        up = worldDown;
        break;
    }

    projectionMatrix_ = glm::perspective(glm::radians(fov_), aspectRatio_, nearPlane_, farPlane_);
    viewMatrix_ = glm::lookAt(position_, position_ + front, up);
    viewProjectionMatrix_ = projectionMatrix_ * viewMatrix_;

    hasChanged_ = true;
    dirtyMatrices_ = false;
}

void Camera::processMouseMovement(float xOffset, float yOffset)
{
    xOffset *= sensitivity;
    yOffset *= sensitivity;

    pitch_ += yOffset;
    pitch_ = std::clamp(pitch_, minPitch, maxPitch);

    yaw_ = std::fmod(yaw_ - xOffset, 360.0f);

    hasChanged_ = true;
    dirtyMatrices_ = true;
}

void Camera::updateMatrices()
{
    using namespace WorldVectors;

    glm::vec3 front{createFront()};
    glm::vec3 right{glm::cross(front, worldUp)};
    right = glm::normalize(right);
    glm::vec3 up{glm::cross(right, front)};

    viewMatrix_ = glm::lookAt(position_, position_ + front, up);
    projectionMatrix_ = glm::perspective(glm::radians(fov_), aspectRatio_, nearPlane_, farPlane_);

    viewProjectionMatrix_ = projectionMatrix_ * viewMatrix_;
    inverseVPMatrix_ = glm::inverse(viewProjectionMatrix_);

    dirtyMatrices_ = false;
}

glm::vec3 Camera::createFront() const
{
    const float pitchRads{glm::radians(pitch_)};
    const float yawRads{glm::radians(yaw_)};
    const float cosPitch{glm::cos(pitchRads)};

    glm::vec3 front;
    front.x = cosPitch * glm::sin(yawRads);
    front.y = glm::sin(pitchRads);
    front.z = cosPitch * glm::cos(yawRads);

    return front;
}

float Camera::getFOV() const
{
    return fov_;
}

const glm::vec3 Camera::getFront() const
{
    return createFront();
}
float Camera::getFarPlane() const
{
    return farPlane_;
}

float Camera::getNearPlane() const
{
    return nearPlane_;
}

void Camera::setFarPlane(float farPlane)
{
    farPlane_ = farPlane;
    dirtyMatrices_ = true;
    hasChanged_ = true;
}

void Camera::setNearPlane(float nearPlane)
{
    nearPlane_ = nearPlane;
    dirtyMatrices_ = true;
    hasChanged_ = true;
}

void Camera::setAspectRatio(float aspectRatio)
{
    aspectRatio_ = aspectRatio;
    dirtyMatrices_ = true;
    hasChanged_ = true;
}

glm::mat4 Camera::getResizedViewProjectionMatrix(float ratio)
{
    if (dirtyMatrices_)
    {
        updateMatrices();
        dirtyMatrices_ = false;
    }

    const glm::mat4 projectionMatrix{glm::perspective(glm::radians(fov_), aspectRatio_, nearPlane_, farPlane_ * ratio)};
    return projectionMatrix * viewMatrix_;
}

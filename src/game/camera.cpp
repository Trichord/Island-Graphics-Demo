#include "game/camera.h"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "utilities/world_vectors.h"

constexpr float maxPitch{85.0f};
constexpr float minPitch{-maxPitch};

Camera::Camera(const Camera::Description &description, float aspectRatio)
    : yaw_{description.yaw}, pitch_{description.pitch}, position_{description.position}, fov_{description.fov},
      nearPlane_{description.nearPlane}, farPlane_{description.farPlane}, aspectRatio_{aspectRatio}
{
}

bool Camera::hasUpdated() const
{
    return hasChanged_;
}

void Camera::clearFlags()
{
    hasChanged_ = false;
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

const glm::mat4 &Camera::getViewMatrix() const
{
    if (dirtyMatrices_)
        updateMatrices();

    return viewMatrix_;
}

const glm::mat4 &Camera::getProjectionMatrix() const
{
    if (dirtyMatrices_)
        updateMatrices();

    return projectionMatrix_;
}

const glm::mat4 &Camera::getViewProjectionMatrix() const
{
    if (dirtyMatrices_)
        updateMatrices();

    return viewProjectionMatrix_;
}

void Camera::setYaw(float yaw)
{
    yaw_ = std::fmod(yaw, 360.0f);
    hasChanged_ = true;
    dirtyMatrices_ = true;
}

void Camera::addYaw(float yaw)
{
    yaw_ = std::fmod(yaw_ + yaw, 360.0f);

    hasChanged_ = true;
    dirtyMatrices_ = true;
}

void Camera::addPitch(float pitch)
{
    pitch_ = std::clamp(pitch_ + pitch, minPitch, maxPitch);

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

// Sets camera to all the world vectors, including ones unreachable through pitch assignments. Useful for cubemap face
// generation.
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
        up = worldFront;
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

void Camera::updateMatrices() const
{
    using namespace WorldVectors;

    glm::vec3 front{createFront()};
    glm::vec3 right{glm::cross(front, worldUp)};
    right = glm::normalize(right);
    glm::vec3 up{glm::cross(right, front)};

    viewMatrix_ = glm::lookAt(position_, position_ + front, up);
    projectionMatrix_ = glm::perspective(glm::radians(fov_), aspectRatio_, nearPlane_, farPlane_);
    viewProjectionMatrix_ = projectionMatrix_ * viewMatrix_;

    dirtyMatrices_ = false;
}

glm::vec3 Camera::createFront() const
{
    const float pitchRads{glm::radians(pitch_)};
    const float yawRads{glm::radians(yaw_)};
    const float cosPitch{glm::cos(pitchRads)};

    glm::vec3 front;
    front.x = glm::cos(yawRads) * cosPitch;
    front.y = glm::sin(pitchRads);
    front.z = glm::sin(yawRads) * cosPitch;

    return front;
}

float Camera::getFOV() const
{
    return fov_;
}

glm::vec3 Camera::getFront() const
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

glm::mat4 Camera::getResizedViewProjectionMatrix(float ratio) const
{
    if (dirtyMatrices_)
        updateMatrices();

    const glm::mat4 projectionMatrix{glm::perspective(glm::radians(fov_), aspectRatio_, nearPlane_, farPlane_ * ratio)};
    return projectionMatrix * viewMatrix_;
}

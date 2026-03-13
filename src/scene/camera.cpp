#include "camera.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_access.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "world_vectors.h"

Camera::Camera(glm::vec3 position, float yaw, float pitch)
    : yaw_{yaw}, pitch_{pitch}, position_{position},
      projectionMatrix_{glm::perspective(glm::radians(cameraDefaultValues::fov), cameraDefaultValues::aspectRatio,
                                        cameraDefaultValues::nearPlane, cameraDefaultValues::farPlane)}
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

void normalizePlane(glm::vec4 &vec)
{
    float length = glm::length(glm::vec3{vec});
    vec /= length;
}

const std::array<glm::vec4, 6> &Camera::getFrustumPlanes()
{
    if (dirtyMatrices_)
    {
        updateMatrices();
        dirtyMatrices_ = false;
    }

    const glm::vec4 row0{glm::row(viewProjectionMatrix_, 0)};
    const glm::vec4 row1{glm::row(viewProjectionMatrix_, 1)};
    const glm::vec4 row2{glm::row(viewProjectionMatrix_, 2)};
    const glm::vec4 row3{glm::row(viewProjectionMatrix_, 3)};

    // Left
    frustumPlanes_[0] = row3 + row0;

    // Right
    frustumPlanes_[1] = row3 - row0;

    // Top
    frustumPlanes_[2] = row3 - row1;

    // Bottom
    frustumPlanes_[3] = row3 + row1;

    // Near
    frustumPlanes_[4] = row3 + row2;

    // Far
    frustumPlanes_[5] = row3 - row2;

    for (auto &vec : frustumPlanes_)
        normalizePlane(vec);

    return frustumPlanes_;
}

void Camera::resetFOV()
{
    projectionMatrix_ = glm::perspective(glm::radians(cameraDefaultValues::fov), cameraDefaultValues::aspectRatio,
                                        cameraDefaultValues::nearPlane, cameraDefaultValues::farPlane);
    hasChanged_ = true;
    dirtyMatrices_ = true;
}

void Camera::setYaw(float newYaw)
{
    yaw_ = std::fmod(newYaw, 360.0f);
    hasChanged_ = true;
    dirtyMatrices_ = true;
}

void Camera::setFOV(float fov, float aspectRatio)
{
    projectionMatrix_ =
        glm::perspective(glm::radians(fov), aspectRatio, cameraDefaultValues::nearPlane, cameraDefaultValues::farPlane);
    hasChanged_ = true;
    dirtyMatrices_ = true;
}

void Camera::setPitch(float newPitch)
{
    using namespace cameraDefaultValues;

    pitch_ = std::clamp(newPitch, minPitch, maxPitch);
    hasChanged_ = true;
    dirtyMatrices_ = true;
}

void Camera::setPosition(const glm::vec3 &newPosition)
{
    position_ = newPosition;
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

    viewMatrix_ = glm::lookAt(position_, position_ + front, up);
    viewProjectionMatrix_ = projectionMatrix_ * viewMatrix_;

    hasChanged_ = true;
    dirtyMatrices_ = false;
}

void Camera::processMouseMovement(float xOffset, float yOffset)
{
    using namespace cameraDefaultValues;

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
    viewProjectionMatrix_ = projectionMatrix_ * viewMatrix_;
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

const glm::vec3 Camera::getFront() const
{
    return createFront();
}
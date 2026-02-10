#include "camera.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_access.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "world_vectors.h"

Camera::Camera(glm::vec3 position, float yaw, float pitch)
    : yaw{yaw}, pitch{pitch}, position{position},
      projectionMatrix{glm::perspective(glm::radians(cameraDefaultValues::fov), cameraDefaultValues::aspectRatio,
                                        cameraDefaultValues::nearPlane, cameraDefaultValues::farPlane)}
{
}

bool Camera::hasUpdated()
{
    if (hasChanged)
    {
        hasChanged = false;
        return true;
    }
    else
        return false;
}

float Camera::getYaw() const
{
    return yaw;
}

float Camera::getPitch() const
{
    return pitch;
}

const glm::vec3 &Camera::getPosition() const
{
    return position;
}

const glm::mat4 &Camera::getViewMatrix()
{
    if (dirtyMatrices)
    {
        updateMatrices();
        dirtyMatrices = false;
    }

    return viewMatrix;
}

const glm::mat4 &Camera::getProjectionMatrix()
{
    return projectionMatrix;
}

const glm::mat4 &Camera::getViewProjectionMatrix()
{
    if (dirtyMatrices)
    {
        updateMatrices();
        dirtyMatrices = false;
    }

    return viewProjectionMatrix;
}

void normalizePlane(glm::vec4 &vec)
{
    float length = glm::length(glm::vec3{vec});
    vec /= length;
}

const std::array<glm::vec4, 6> &Camera::getFrustumPlanes()
{
    if (dirtyMatrices)
    {
        updateMatrices();
        dirtyMatrices = false;
    }

    const glm::vec4 row0{glm::row(viewProjectionMatrix, 0)};
    const glm::vec4 row1{glm::row(viewProjectionMatrix, 1)};
    const glm::vec4 row2{glm::row(viewProjectionMatrix, 2)};
    const glm::vec4 row3{glm::row(viewProjectionMatrix, 3)};

    // Left
    frustumPlanes[0] = row3 + row0;

    // Right
    frustumPlanes[1] = row3 - row0;

    // Top
    frustumPlanes[2] = row3 - row1;

    // Bottom
    frustumPlanes[3] = row3 + row1;

    // Near
    frustumPlanes[4] = row3 + row2;

    // Far
    frustumPlanes[5] = row3 - row2;

    for (auto &vec : frustumPlanes)
        normalizePlane(vec);

    return frustumPlanes;
}

void Camera::resetFOV()
{
    projectionMatrix = glm::perspective(glm::radians(cameraDefaultValues::fov), cameraDefaultValues::aspectRatio,
                                        cameraDefaultValues::nearPlane, cameraDefaultValues::farPlane);
    hasChanged = true;
    dirtyMatrices = true;
}

void Camera::setYaw(float newYaw)
{
    yaw = std::fmod(newYaw, 360.0f);
    hasChanged = true;
    dirtyMatrices = true;
}

void Camera::setFOV(float fov, float aspectRatio)
{
    projectionMatrix =
        glm::perspective(glm::radians(fov), aspectRatio, cameraDefaultValues::nearPlane, cameraDefaultValues::farPlane);
    hasChanged = true;
    dirtyMatrices = true;
}

void Camera::setPitch(float newPitch)
{
    using namespace cameraDefaultValues;

    pitch = std::clamp(newPitch, minPitch, maxPitch);
    hasChanged = true;
    dirtyMatrices = true;
}

void Camera::setPosition(const glm::vec3 &newPosition)
{
    position = newPosition;
    hasChanged = true;
    dirtyMatrices = true;
}

// Maps world orientation to cubemap directions
void Camera::setWorldOrientation(WorldOrientation orientation)
{
    using enum WorldOrientation;
    using namespace WorldVectors;

    glm::vec3 front;
    glm::vec3 up;

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

    viewMatrix = glm::lookAt(position, position + front, up);
    viewProjectionMatrix = projectionMatrix * viewMatrix;

    hasChanged = true;
    dirtyMatrices = false;
}

void Camera::processMouseMovement(float xOffset, float yOffset)
{
    using namespace cameraDefaultValues;

    xOffset *= sensitivity;
    yOffset *= sensitivity;

    pitch += yOffset;
    pitch = std::clamp(pitch, minPitch, maxPitch);

    yaw = std::fmod(yaw - xOffset, 360.0f);

    hasChanged = true;
    dirtyMatrices = true;
}

void Camera::processKeyboard(CameraMovement direction, float deltaTime, bool sprint)
{
    using enum CameraMovement;
    using namespace WorldVectors;
    constexpr float cameraSprintSpeed{2.5f};

    float boost{sprint ? cameraSprintSpeed : 1.0f};
    float velocity{cameraDefaultValues::speed * deltaTime * boost};

    // Using local scope to minimize work
    switch (direction)
    {
    case FORWARD: {
        glm::vec3 front{createFront()};

        position += front * velocity;
        break;
    }

    case BACKWARD: {
        glm::vec3 front{createFront()};

        position -= front * velocity;
        break;
    }

    case LEFT: {
        glm::vec3 front{createFront()};
        glm::vec3 right{glm::cross(front, worldUp)};
        right = glm::normalize(right);

        position -= right * velocity;
        break;
    }

    case RIGHT: {
        glm::vec3 front{createFront()};
        glm::vec3 right{glm::cross(front, worldUp)};
        right = glm::normalize(right);

        position += right * velocity;
        break;
    }

    case UP:
        position += worldUp * velocity;
        break;

    case DOWN:
        position -= worldUp * velocity;
        break;
    }

    hasChanged = true;
    dirtyMatrices = true;
}

void Camera::updateMatrices()
{
    using namespace WorldVectors;

    glm::vec3 front{createFront()};
    glm::vec3 right{glm::cross(front, worldUp)};
    right = glm::normalize(right);
    glm::vec3 up{glm::cross(right, front)};

    viewMatrix = glm::lookAt(position, position + front, up);
    viewProjectionMatrix = projectionMatrix * viewMatrix;
}

glm::vec3 Camera::createFront()
{
    float pitchRads{glm::radians(pitch)};
    float yawRads{glm::radians(yaw)};
    float cosPitch{glm::cos(pitchRads)};

    glm::vec3 front;
    front.x = cosPitch * glm::sin(yawRads);
    front.y = glm::sin(pitchRads);
    front.z = cosPitch * glm::cos(yawRads);

    return front;
}
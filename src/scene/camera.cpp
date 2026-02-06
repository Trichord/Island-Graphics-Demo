#include "camera.h"

#include <algorithm>
#include <array>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_access.hpp"
#include "glm/gtc/matrix_transform.hpp"

Camera::Camera(glm::vec3 position, float yaw, float pitch)
    : yaw{yaw}, pitch{pitch}, position{position},
      projectionMatrix{glm::perspective(glm::radians(cameraDefaultValues::zoom), cameraDefaultValues::aspectRatio,
                                        cameraDefaultValues::nearPlane, cameraDefaultValues::farPlane)}
{
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

const glm::mat4 &Camera::getViewMatrixNoTranslation()
{
    if (dirtyMatrices)
    {
        updateMatrices();
        dirtyMatrices = false;
    }

    return viewMatrixNoTranslation;
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

void Camera::processKeyboard(CameraMovement direction, float deltaTime, bool sprint)
{
    using enum CameraMovement;
    constexpr float cameraSprintSpeed{2.5f};

    float boost{sprint ? cameraSprintSpeed : 1.0f};
    float velocity{cameraDefaultValues::speed * deltaTime * boost};

    // Using local scope to minimize work
    switch (direction)
    {
    case FORWARD: {
        glm::vec3 front{createFront()};
        position += front * velocity;
    }
    break;

    case BACKWARD: {
        glm::vec3 front{createFront()};
        position -= front * velocity;
    }
    break;

    case LEFT: {
        glm::vec3 front{createFront()};
        glm::vec3 right{glm::cross(front, cameraDefaultValues::worldUp)};
        right = glm::normalize(right);

        position -= right * velocity;
    }
    break;

    case RIGHT: {
        glm::vec3 front{createFront()};
        glm::vec3 right{glm::cross(front, cameraDefaultValues::worldUp)};
        right = glm::normalize(right);

        position += right * velocity;
    }
    break;

    case UP:
        position += cameraDefaultValues::worldUp * velocity;

        break;

    case DOWN:
        position -= cameraDefaultValues::worldUp * velocity;

        break;
    }

    hasChanged = true;
    dirtyMatrices = true;
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

const glm::vec3 &Camera::getPosition() const
{
    return position;
}

void Camera::setPosition(const glm::vec3 &&newPosition)
{
    position = newPosition;
    dirtyMatrices = true;
}

void Camera::setPitch(float newPitch)
{
    using namespace cameraDefaultValues;

    pitch = std::clamp(newPitch, minPitch, maxPitch);
}

void Camera::setYaw(float newYaw)
{
    yaw = std::fmod(newYaw, 360.0f);
}

void Camera::setWorldOrientation(WorldOrientation orientation)
{
    using namespace cameraDefaultValues;
    using enum WorldOrientation;

    glm::vec3 front;
    glm::vec3 right;
    glm::vec3 up;

    switch (orientation)
    {
    case UP:
        front = worldUp;
        right = worldRight;
        up = worldBack;
        break;

    case DOWN:
        front = worldDown;
        right = worldRight;
        up = worldFront;
        break;

    case RIGHT:
        front = worldRight;
        right = worldBack;
        up = worldUp;
        break;

    case LEFT:
        front = worldLeft;
        right = worldFront;
        up = worldUp;
        break;

    case FRONT:
        front = worldFront;
        right = worldRight;
        up = worldUp;
        break;

    case BACK:
        front = worldBack;
        right = worldLeft;
        up = worldUp;
        break;
    }

    viewMatrix = glm::lookAt(position, position + front, up);
    viewMatrixNoTranslation = glm::mat4{glm::mat3{viewMatrix}};
    viewProjectionMatrix = projectionMatrix * viewMatrix;

    dirtyMatrices = false;
    hasChanged = false;
}

void Camera::updateMatrices()
{
    glm::vec3 front{createFront()};
    glm::vec3 right{glm::cross(front, cameraDefaultValues::worldUp)};
    right = glm::normalize(right);
    glm::vec3 up{glm::cross(right, front)};

    viewMatrix = glm::lookAt(position, position + front, up);
    viewMatrixNoTranslation = glm::mat4{glm::mat3{viewMatrix}};
    viewProjectionMatrix = projectionMatrix * viewMatrix;
}

const glm::mat4 &Camera::getProjectionMatrix()
{
    return projectionMatrix;
}

void normalizePlane(glm::vec4 &vec)
{
    float length = glm::length(glm::vec3{vec});
    vec /= length;
}

const std::array<glm::vec4, 6> &Camera::getFrustumPlanes()
{
    if (!dirtyMatrices)
        return frustumPlanes;

    updateMatrices();
    dirtyMatrices = false;

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

void Camera::setAspectRatio(float ratio)
{
    projectionMatrix = glm::perspective(glm::radians(cameraDefaultValues::zoom), ratio, cameraDefaultValues::nearPlane,
                                        cameraDefaultValues::farPlane);
}
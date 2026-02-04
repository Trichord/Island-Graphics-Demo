#include "camera.h"

#include <algorithm>
#include <array>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_access.hpp"
#include "glm/gtc/matrix_transform.hpp"

Camera::Camera(glm::vec3 position_, float yaw, float pitch)
    : position{position_},
      rotationMatrix{glm::rotate(glm::mat4{1.0f}, glm::radians(yaw), glm::vec3{0.0f, 1.0f, 0.0f}) *
                     glm::rotate(glm::mat4{1.0f}, glm::radians(pitch), glm::vec3{1.0f, 0.0f, 0.0f})},
      projectionMatrix{glm::perspective(glm::radians(cameraDefaultValues::zoom), cameraDefaultValues::aspectRatio,
                                        cameraDefaultValues::nearPlane, cameraDefaultValues::farPlane)},
      viewProjectionMatrix{glm::mat4{1.0f}}, pitch_{pitch}
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

void Camera::processKeyboard(CameraMovement direction, float deltaTime, bool sprint)
{
    using enum CameraMovement;
    constexpr float cameraSprintSpeed{2.5f};

    if (dirtyMatrices)
    {
        updateMatrices();
        dirtyMatrices = false;
    }

    glm::vec3 right{rotationMatrix[0]};
    glm::vec3 front{-rotationMatrix[2]};

    float velocityBoost{sprint ? cameraSprintSpeed : 1.0f};

    float velocity{cameraDefaultValues::speed * deltaTime};
    if (direction == FORWARD)
        position += front * velocity * velocityBoost;
    if (direction == BACKWARD)
        position -= front * velocity * velocityBoost;
    if (direction == LEFT)
        position -= right * velocity * velocityBoost;
    if (direction == RIGHT)
        position += right * velocity * velocityBoost;
    if (direction == UP)
        position += cameraDefaultValues::worldUp * velocity * velocityBoost;
    if (direction == DOWN)
        position -= cameraDefaultValues::worldUp * velocity * velocityBoost;

    hasChanged = true;
    dirtyMatrices = true;
}

// processes input received from a mouse input system. Expects the offset value
// in both the x and y direction.
void Camera::processMouseMovement(float xOffset, float yOffset, bool constrainPitch)
{
    xOffset *= cameraDefaultValues::sensitivity;
    yOffset *= cameraDefaultValues::sensitivity;
    pitch_ += yOffset;

    if (constrainPitch)
        pitch_ = std::clamp(pitch_, -85.0f, 85.0f);

    // pitch
    rotationMatrix = glm::rotate(glm::mat4{1.0f}, glm::radians(-xOffset), glm::vec3{0.0f, 1.0f, 0.0f}) *
                     glm::rotate(rotationMatrix, glm::radians(yOffset), glm::vec3{1.0f, 0.0f, 0.0f});

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

void Camera::updateMatrices()
{
    viewMatrix = glm::transpose(rotationMatrix) * glm::translate(glm::mat4{1.0f}, -position);
    viewMatrixNoTranslation = glm::transpose(rotationMatrix);
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

void Camera::setAspectRatio(float ratio)
{
    projectionMatrix = glm::perspective(glm::radians(cameraDefaultValues::zoom), ratio, cameraDefaultValues::nearPlane,
                                        cameraDefaultValues::farPlane);
}
#include "../headers/camera.h"
#include "../headers/config_window.h"

#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

Camera::Camera() : projectionMatrix{createProjectionMatrix()} {
  updateCameraVectors();
  dirtyMatrices = true;
}

Camera::Camera(glm::vec3 pos, glm::vec3 u, float y, float p)
    : front{0.0f, 0.0f, -1.0f}, projectionMatrix{createProjectionMatrix()} {
  position = pos;
  worldUp = u;
  yaw = y;
  pitch = p;
  updateCameraVectors();
  dirtyMatrices = true;
}

Camera::Camera(float posX, float posY, float posZ, float upX, float upY,
               float upZ, float y, float p)
    : front{0.0f, 0.0f, -1.0f},
      projectionMatrix{createProjectionMatrix()},
      position{posX, posY, posZ},
      worldUp{upX, upY, upZ},
      yaw{y},
      pitch{p} {
  updateCameraVectors();
  dirtyMatrices = true;
}

const glm::mat4& Camera::getViewMatrix() {
  if (dirtyMatrices) {
    updateCameraMatrices();
    dirtyMatrices = false;
  }

  return viewMatrix;
}

const glm::mat4& Camera::getViewNoTranslationMatrix() {
  if (dirtyMatrices) {
    updateCameraMatrices();
    dirtyMatrices = false;
  }

  return viewNoTranslationMatrix;
}

void Camera::processKeyboard(CameraMovement direction, float deltaTime) {
  using enum CameraMovement;
  float velocity{movementSpeed * deltaTime};
  if (direction == FORWARD) position += front * velocity;
  if (direction == BACKWARD) position -= front * velocity;
  if (direction == LEFT) position -= right * velocity;
  if (direction == RIGHT) position += right * velocity;
  if (direction == UP) position += up * velocity;
  if (direction == DOWN) position -= up * velocity;

  hasChanged = true;
  dirtyMatrices = true;
}

// processes input received from a mouse input system. Expects the offset value
// in both the x and y direction.
void Camera::processMouseMovement(float xoffset, float yoffset,
                                  bool constrainPitch) {
  xoffset *= mouseSensitivity;
  yoffset *= mouseSensitivity;

  yaw += xoffset;
  pitch += yoffset;

  // make sure that when pitch is out of bounds, screen doesn't get flipped
  if (constrainPitch) {
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
  }

  // keep yaw from drifting
  if (yaw < 0.0f || yaw > 360.0f) yaw = std::fmod(yaw, 360.0f);

  // update Front, Right and Up Vectors using the updated Euler angles
  updateCameraVectors();
  hasChanged = true;
  dirtyMatrices = true;
}

bool Camera::hasUpdated() {
  if (hasChanged) {
    hasChanged = false;
    return true;
  } else
    return false;
}

const glm::vec3& Camera::getPosition() const { return position; }

// calculates the front vector from the Camera's (updated) Euler angles
void Camera::updateCameraVectors() {
  float cosPitchRad{glm::cos(glm::radians(pitch))};
  float sinPitchRad{glm::sin(glm::radians(pitch))};
  float yawRad{glm::radians(yaw)};

  glm::vec3 newFront{glm::cos(yawRad) * cosPitchRad, sinPitchRad,
                     glm::sin(yawRad) * cosPitchRad};

  front = glm::normalize(newFront);
  right = glm::normalize(glm::cross(front, worldUp));
  up = glm::normalize(glm::cross(right, front));
}

void Camera::updateCameraMatrices() {
  viewMatrix = glm::lookAt(position, position + front, up);
  viewNoTranslationMatrix = glm::mat4{glm::mat3{viewMatrix}};
}

const glm::mat4& Camera::getProjectionMatrix() { return projectionMatrix; }

glm::mat4 Camera::createProjectionMatrix() {
  return glm::perspective(
      glm::radians(defaultCameraZoom),
      (float)ConfigWindow::globalGraphicsSettings.screenWidth /
          (float)ConfigWindow::globalGraphicsSettings.screenHeight,
      defaultCameraNearPlane, defaultCameraFarPlane);
}
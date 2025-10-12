#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#define CAMERA_YAW 90.0f
#define CAMERA_PITCH 0.0f
#define CAMERA_SPEED 5.0f
#define CAMERA_SENSITIVITY 0.1f

// Defines several possible options for camera movement. Used as abstraction to
// stay away from window-system specific input methods
enum Camera_Movement { FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN };

class Camera {
 public:
  Camera();
  Camera(glm::vec3 pos, glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f),
         float y = CAMERA_YAW, float p = CAMERA_PITCH);
  Camera(float posX, float posY, float posZ, float upX, float upY, float upZ,
         float yaw, float pitch);

  const glm::mat4& getViewMatrix();
  const glm::mat4& getViewNoTranslationMatrix();

  void processKeyboard(Camera_Movement direction, float deltaTime);
  void processMouseMovement(float xoffset, float yoffset,
                            GLboolean constrainPitch = true);
  bool hasUpdated();
  const glm::vec3& getPosition() const;

 private:
  glm::vec3 position = glm::vec3(0.0f);
  glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
  glm::vec3 right;
  glm::vec3 up;
  glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
  float yaw = CAMERA_YAW;
  float pitch = CAMERA_PITCH;
  float movementSpeed = CAMERA_SPEED;
  float mouseSensitivity = CAMERA_SENSITIVITY;
  bool hasChanged = false;
  bool dirtyMatrices = true;
  glm::mat4 viewMatrix;
  glm::mat4 viewNoTranslationMatrix;
  void updateCameraVectors();
  void updateCameraMatrices();
};
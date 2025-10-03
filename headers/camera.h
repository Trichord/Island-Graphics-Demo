#pragma once

#include <glad/glad.h>

#include <glm/glm.hpp>

// Defines several possible options for camera movement. Used as abstraction to
// stay away from window-system specific input methods
enum Camera_Movement { FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN };

class Camera {
 public:
  Camera();
  Camera(glm::vec3 pos, glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f),
         float y = YAW, float p = PITCH);
  Camera(float posX, float posY, float posZ, float upX, float upY, float upZ,
         float yaw, float pitch);

  glm::vec3 position;
  glm::vec3 front;
  glm::vec3 up;
  glm::vec3 right;
  glm::vec3 worldUp;
  float yaw;
  float pitch;
  float movementSpeed;
  float mouseSensitivity;
  float zoom;

  const glm::mat4& getViewMatrix();
  const glm::mat4& getViewNoTranslationMatrix();

  void processKeyboard(Camera_Movement direction, float deltaTime);
  void processMouseMovement(float xoffset, float yoffset,
                            GLboolean constrainPitch = true);
  void processMouseScroll(float yoffset);
  bool hasUpdated();

 private:
  inline static const float YAW = -90.0f;
  inline static const float PITCH = 0.0f;
  inline static const float SPEED = 2.5f;
  inline static const float SENSITIVITY = 0.1f;
  inline static const float ZOOM = 45.0f;
  bool hasChanged = false;
  bool dirtyMatrices = true;
  glm::mat4 viewMatrix;
  glm::mat4 viewNoTranslationMatrix;
  void updateCameraVectors();
  void updateCameraMatrices();
};
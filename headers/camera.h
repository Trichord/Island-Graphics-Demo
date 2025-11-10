#pragma once

#include <glm/glm.hpp>

inline constexpr float defaultCameraYaw{90.0f};
inline constexpr float defaultCameraPitch{0.0f};
inline constexpr float defaultCameraSpeed{5.0f};
inline constexpr float defaultCameraSensitivity{0.1f};
inline constexpr float defaultCameraZoom{45.0f};
inline constexpr float defaultCameraNearPlane{0.1f};
inline constexpr float defaultCameraFarPlane{600.0f};

enum class CameraMovement { FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN };

class Camera {
 public:
  Camera();
  Camera(glm::vec3 pos, glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f),
         float y = defaultCameraYaw, float p = defaultCameraPitch);
  Camera(float posX, float posY, float posZ, float upX, float upY, float upZ,
         float yaw, float pitch);

  const glm::mat4& getViewMatrix();
  const glm::mat4& getViewMatrixNoTranslation();
  const glm::mat4& getProjectionMatrix();

  void processKeyboard(CameraMovement direction, float deltaTime);
  void processMouseMovement(float xoffset, float yoffset,
                            bool constrainPitch = true);
  bool hasUpdated();
  const glm::vec3& getPosition() const;

 private:
  float yaw = defaultCameraYaw;
  float pitch = defaultCameraPitch;
  float movementSpeed = defaultCameraSpeed;
  float mouseSensitivity = defaultCameraSensitivity;
  bool hasChanged{false};
  bool dirtyMatrices{true};
  glm::vec3 position{0.0f};
  glm::vec3 front{0.0f, 0.0f, -1.0f};
  glm::vec3 right;
  glm::vec3 up;
  glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
  glm::mat4 viewMatrix{1.0f};
  glm::mat4 viewMatrixNoTranslation{1.0f};
  glm::mat4 projectionMatrix{1.0f};
  glm::mat4 createProjectionMatrix();
  void updateCameraVectors();
  void updateCameraMatrices();
};
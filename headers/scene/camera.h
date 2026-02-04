#pragma once

#include <array>

#include "glm/glm.hpp"

namespace cameraDefaultValues
{
inline constexpr float yaw{90.0f};
inline constexpr float pitch{0.0f};
inline constexpr float speed{5.0f};
inline constexpr float sensitivity{0.1f};
inline constexpr float zoom{45.0f};
inline constexpr float nearPlane{0.1f};
inline constexpr float farPlane{475.0f};
inline constexpr float aspectRatio{16.0f / 9.0f};
inline constexpr glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
} // namespace cameraDefaultValues

enum class CameraMovement
{
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

class Camera
{
  public:
    Camera(glm::vec3 position = glm::vec3{0.0f}, float yaw = cameraDefaultValues::yaw,
           float pitch = cameraDefaultValues::pitch);

    const glm::mat4 &getViewMatrix();
    const glm::mat4 &getViewMatrixNoTranslation();
    const glm::mat4 &getProjectionMatrix();
    const glm::mat4 &getViewProjectionMatrix();
    const std::array<glm::vec4, 6> &getFrustumPlanes();

    void processKeyboard(CameraMovement direction, float deltaTime, bool sprint = false);
    void processMouseMovement(float xOffset, float yOffset, bool constrainPitch = true);
    bool hasUpdated();
    const glm::vec3 &getPosition() const;
    void setPosition(const glm::vec3 &&newPosition);
    void setAspectRatio(float ratio);

  private:
    glm::vec3 position{0.0f};
    glm::mat4 rotationMatrix{1.0f};
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 viewMatrixNoTranslation{1.0f};
    glm::mat4 projectionMatrix{1.0f};
    glm::mat4 viewProjectionMatrix{1.0f};
    float pitch_{0.0f};

    bool hasChanged{false};
    bool dirtyMatrices{true};

    // Left, Right, Top, Bottom, Near, Far
    std::array<glm::vec4, 6> frustumPlanes{glm::vec4{}, glm::vec4{}, glm::vec4{},
                                           glm::vec4{}, glm::vec4{}, glm::vec4{}};

    void updateMatrices();
};
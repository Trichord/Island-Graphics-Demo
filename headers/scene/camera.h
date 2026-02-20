#pragma once

#include <array>

#include "glm/glm.hpp"
#include "world_vectors.h"

namespace cameraDefaultValues
{
constexpr float yaw{90.0f};
constexpr float pitch{0.0f};
constexpr float maxPitch{85.0f};
constexpr float minPitch{-maxPitch};

constexpr float speed{5.0f};
constexpr float sensitivity{0.1f};

constexpr float fov{45.0f};
constexpr float nearPlane{0.1f};
constexpr float farPlane{850.0f};
constexpr float aspectRatio{16.0f / 9.0f};
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

    [[nodiscard("Consumes 'updated' flag")]] bool hasUpdated();

    float getYaw() const;
    float getPitch() const;
    const glm::vec3 &getPosition() const;

    const glm::mat4 &getViewMatrix();
    const glm::mat4 &getProjectionMatrix();
    const glm::mat4 &getViewProjectionMatrix();
    const std::array<glm::vec4, 6> &getFrustumPlanes();

    void resetFOV();
    void setYaw(float newYaw);
    void setPitch(float newPitch);
    void setFOV(float fov, float aspectRatio);
    void setPosition(const glm::vec3 &newPosition);
    void setWorldOrientation(WorldOrientation orientation);

    void processMouseMovement(float xOffset, float yOffset);
    void processKeyboard(CameraMovement direction, float deltaTime, bool sprint = false);

  private:
    float yaw{0.0f};
    float pitch{0.0f};
    glm::vec3 position{0.0f};
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projectionMatrix{1.0f};
    glm::mat4 viewProjectionMatrix{1.0f};

    bool hasChanged{false};
    bool dirtyMatrices{true};

    // Left, Right, Top, Bottom, Near, Far
    std::array<glm::vec4, 6> frustumPlanes{glm::vec4{}, glm::vec4{}, glm::vec4{},
                                           glm::vec4{}, glm::vec4{}, glm::vec4{}};

    void updateMatrices();
    glm::vec3 createFront();
};
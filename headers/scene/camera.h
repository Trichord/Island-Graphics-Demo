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

constexpr float sensitivity{0.1f};

constexpr float fov{45.0f};
constexpr float nearPlane{0.1f};
constexpr float farPlane{2000.0f};
constexpr float aspectRatio{16.0f / 9.0f};
} // namespace cameraDefaultValues

class Camera
{
  public:
    Camera(glm::vec3 position = glm::vec3{0.0f}, float yaw = cameraDefaultValues::yaw,
           float pitch = cameraDefaultValues::pitch);

    [[nodiscard("Consumes 'updated' flag")]] bool hasUpdated();

    float getYaw() const;
    float getPitch() const;
    const glm::vec3 getFront() const;
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

  private:
    float yaw_{0.0f};
    float pitch_{0.0f};
    glm::vec3 position_{0.0f};
    glm::mat4 viewMatrix_{1.0f};
    glm::mat4 projectionMatrix_{1.0f};
    glm::mat4 viewProjectionMatrix_{1.0f};

    bool hasChanged_{false};
    bool dirtyMatrices_{true};

    // Left, Right, Top, Bottom, Near, Far
    std::array<glm::vec4, 6> frustumPlanes_{glm::vec4{}, glm::vec4{}, glm::vec4{},
                                           glm::vec4{}, glm::vec4{}, glm::vec4{}};

    void updateMatrices();
    glm::vec3 createFront() const;
};
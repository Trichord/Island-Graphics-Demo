#pragma once

#include <glm/glm.hpp>

#include "utilities/world_vectors.h"

class Camera
{
  public:
    Camera(glm::vec3 position = glm::vec3{0.0f}, float yaw = -90.0f, float pitch = 0.0f);

    float getFOV() const;
    float getYaw() const;
    float getPitch() const;
    float getFarPlane() const;
    float getNearPlane() const;
    float getAspectRatio() const;

    const glm::vec3 getFront() const;
    const glm::vec3 &getPosition() const;
    const glm::mat4 &getViewMatrix() const;
    const glm::mat4 &getProjectionMatrix() const;
    const glm::mat4 &getViewProjectionMatrix() const;
    glm::mat4 getResizedViewProjectionMatrix(float ratio) const;

    void setFOV(float fov);
    void setYaw(float yaw);
    void setPitch(float pitch);
    void setFarPlane(float farPlane);
    void setNearPlane(float nearPlane);
    void setAspectRatio(float aspectRatio);
    void setPosition(const glm::vec3 &position);
    void setWorldOrientation(WorldOrientation orientation);

    void processMouseMovement(float xOffset, float yOffset);

    void clearFlags();
    [[nodiscard]] bool hasUpdated() const;
    
  private:
    float yaw_{-90.0f};
    float pitch_{0.0f};
    glm::vec3 position_{0.0f};

    // Cached matrices
    mutable glm::mat4 viewMatrix_{1.0f};
    mutable glm::mat4 projectionMatrix_{1.0f};
    mutable glm::mat4 viewProjectionMatrix_{1.0f};
    mutable glm::mat4 inverseVPMatrix_{1.0f};

    // State tracking
    mutable bool hasChanged_{false};
    mutable bool dirtyMatrices_{true};

    // Frustum
    float fov_{45.0f};
    float nearPlane_{0.1f};
    float farPlane_{1000.0f};
    float aspectRatio_{16.0f / 9.0f};

    void updateMatrices() const;
    glm::vec3 createFront() const;
};
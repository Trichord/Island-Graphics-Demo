#pragma once

#include "glm/glm.hpp"
#include "world_vectors.h"

class Camera
{
  public:
    Camera(glm::vec3 position = glm::vec3{0.0f}, float yaw = 90.0f, float pitch = 0.0f);

    [[nodiscard("Consumes 'updated' flag")]] bool hasUpdated();

    float getFOV() const;
    float getYaw() const;
    float getPitch() const;
    float getFarPlane() const;
    float getNearPlane() const;
    float getAspectRatio() const;

    const glm::vec3 getFront() const;
    const glm::vec3 &getPosition() const;
    const glm::mat4 &getViewMatrix();
    const glm::mat4 &getProjectionMatrix();
    const glm::mat4 &getViewProjectionMatrix();
    glm::mat4 getResizedViewProjectionMatrix(float ratio);

    void setFOV(float fov);
    void setYaw(float newYaw);
    void setPitch(float newPitch);
    void setFarPlane(float farPlane);
    void setNearPlane(float nearPlane);
    void setAspectRatio(float aspectRatio);
    void setPosition(const glm::vec3 &newPosition);
    void setWorldOrientation(WorldOrientation orientation);

    void processMouseMovement(float xOffset, float yOffset);

  private:
    // Basic angles
    float yaw_{0.0f};
    float pitch_{0.0f};

    // Cached matrices
    glm::vec3 position_{0.0f};
    glm::mat4 viewMatrix_{1.0f};
    glm::mat4 projectionMatrix_{1.0f};
    glm::mat4 viewProjectionMatrix_{1.0f};
    glm::mat4 inverseVPMatrix_{1.0f};

    // State tracking
    bool hasChanged_{false};
    bool dirtyMatrices_{true};

    // Frustum
    float fov_{45.0f};
    float nearPlane_{0.1f};
    float farPlane_{2000.0f};
    float aspectRatio_{16.0f / 9.0f};
    
    void updateMatrices();
    glm::vec3 createFront() const;
};
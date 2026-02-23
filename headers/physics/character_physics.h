#pragma once

#include "capsule.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "glm/glm.hpp"
#include "scene.h"
#include "spatial_grid.h"

enum CharacterMovement : std::uint8_t
{
    FORWARD = 1U,
    BACKWARD = 2U,
    LEFT = 4U,
    RIGHT = 8U,
    NONE = 16U
};

namespace physicsDefaultValues
{
constexpr float surfaceFriction{5.0f};
constexpr float gravityForce{70.0f};
constexpr float playerWidth{0.25f};
constexpr float playerHeight{2.0f};
constexpr float maxGroundVelocity{6.0f};
constexpr float playerMaxAcceleration{10.0f * maxGroundVelocity};
constexpr float playerMaxSpeed{5.0f};
constexpr float playerMinSpeed{0.2f};
constexpr float minSlope{0.3f};
} // namespace physicsDefaultValues

class CharacterPhysics
{
  public:
    CharacterPhysics() = default;
    CharacterPhysics(Scene &scene);

    void update(std::uint8_t direction, const glm::vec3 &front, float deltaTime);

    glm::vec3 getPosition() const;
    const glm::vec3 &getPreviousPosition() const;
    void setPosition(const glm::vec3 &newPosition);

  private:
    struct RayCast
    {
        glm::vec3 intersection{0.0f};
        glm::vec3 normal{0.0f};
        float distance{0.0f};
    };

    glm::vec3 previousPosition{0.0f};
    glm::vec3 resetPosition{0.0f};
    float resetHeight{0.0f};

    Capsule capsule;

    SpatialGrid terrainGrid;
    std::vector<int> triangleCollisions;
    std::vector<glm::vec3> meshVertices;

    RayCast rayCast(const glm::vec3 &position);
    void resolveMovementCollisions();
    void resolveGravityCollisions(float deltaTime);
    std::optional<glm::vec3> checkCollisionNormal();

    // Accelerate function from Valve's Source engine
    glm::vec3 accelerate(const glm::vec3 &wishDir, float wishSpeed, float accel, float deltaTime);
};
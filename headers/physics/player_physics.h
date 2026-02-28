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
constexpr float playerWidth{0.5f};
constexpr float playerHeight{2.0f};
constexpr float playerMaxAcceleration{24.0f};
constexpr float playerMaxSpeed{5.0f};
constexpr float playerMinSpeed{0.2f};
constexpr float maxVerticalVelocity{0.1f};
} // namespace physicsDefaultValues

class PlayerPhysics
{
  public:
    PlayerPhysics() = default;
    PlayerPhysics(Scene &scene);

    void update(std::uint8_t direction, const glm::vec3 &front, float deltaTime);

    glm::vec3 getPosition() const;
    const glm::vec3 &getPreviousPosition() const;
    void setPosition(const glm::vec3 &newPosition);
    void initializeResources(Scene &scene);

  private:
    glm::vec3 previousPosition_{0.0f};
    glm::vec3 resetPosition_{0.0f};
    float resetHeight_{0.0f};

    Capsule capsule_;

    SpatialGrid terrainGrid_;
    SpatialGrid modelGrid_;
    std::vector<int> triangleCollisions_;
    std::vector<glm::vec3> terrainVertices_;
    std::vector<glm::vec3> modelVertices_;

    // Accelerate function from Valve's Source engine
    void movePlayer(std::uint8_t direction, const glm::vec3 &front, float deltaTime);
    glm::vec3 accelerate(const glm::vec3 &wishDir, float wishSpeed, float accel);
   
    void resolveMovementCollisions();
    void resolveGravityCollisions(float deltaTime);
    std::optional<glm::vec3> getCollisionNormal(SpatialGrid &grid, std::vector<glm::vec3>& vertices);

    void createModelGrid(Scene &scene);
    void createTerrainGrid(Scene &scene);
};
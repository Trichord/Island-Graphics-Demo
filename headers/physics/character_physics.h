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

namespace defaultPhysicsValues
{
constexpr float surfaceFriction{5.0f};
constexpr float gravity{20.0f};
constexpr float playerWidth{0.5f};
constexpr float playerHeight{2.0f};
constexpr float maxGroundVelocity{6.0f};
constexpr float playerMaxAcceleration{10.0f * maxGroundVelocity};
constexpr float playerMaxSpeed{5.0f};
constexpr float playerMinSpeed{0.2f};
constexpr float minSlope{0.45f};
} // namespace defaultPhysicsValues

class CharacterPhysics
{
  public:
    CharacterPhysics() = default;
    CharacterPhysics(Scene &scene);

    void update(std::uint8_t direction, const glm::vec3 &front, float deltaTime);

    const glm::vec3 &getPosition() const;
    void setPosition(const glm::vec3 &newPosition);

  private:
    glm::vec3 capsuleVelocity{0.0f};
    glm::vec3 capsulePosition{0.0f};

    Capsule capsule;

    SpatialGrid terrainGrid;
    std::vector<int> triangleCollisions;
    std::vector<glm::vec3> meshVertices;

    std::optional<glm::vec3> checkCollisionNormal();
    void resolveCollisions();

    // Accelerate function from Valve's Source engine
    void accelerate(glm::vec3 &wishDir, float wishSpeed, float accel, float deltaTime);

    // Sliding physics credit: Wicked Engine https://wickedengine.net/2020/04/capsule-collision-detection
    void slide(const glm::vec3 &normal);
};
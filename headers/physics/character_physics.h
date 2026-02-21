#pragma once

#include "capsule.h"

#include <vector>

#include "glm/glm.hpp"
#include "scene.h"
#include "spatial_grid.h"

enum class CharacterMovement
{
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    NONE
};

namespace defaultPhysicsValues
{
constexpr float drag{0.2f};
constexpr float gravity{9.8f};
constexpr float playerWidth{0.5f};
constexpr float playerHeight{2.0f};
constexpr float sprintMultiplier{2.0f};
constexpr float playerMovementSpeed{3.0f};
constexpr float maxVelocity{5.0f};
constexpr float minSlope{0.3f};
} // namespace defaultPhysicsValues

class CharacterPhysics
{
  public:
    CharacterPhysics() = default;
    CharacterPhysics(Scene &scene);

    void update(CharacterMovement direction, const glm::vec3 &front, float deltaTime);

    const glm::vec3 &getPosition() const;
    void setPosition(const glm::vec3 &newPosition);

  private:
    glm::vec3 velocity{0.0f};
    glm::vec3 position{0.0f};

    Capsule capsule;

    SpatialGrid terrainGrid;
    std::vector<int> triangleCollisions;
    std::vector<glm::vec3> meshVertices;
};
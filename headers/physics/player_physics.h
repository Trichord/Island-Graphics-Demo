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
    std::vector<int> collisionTriangles_;
    std::vector<glm::vec3> terrainVertices_;
    std::vector<glm::vec3> modelVertices_;

    void movePlayer(std::uint8_t direction, const glm::vec3 &front, float deltaTime);

    // Accelerate function from Valve's Source engine, slideMove from Quake
    glm::vec3 accelerate(const glm::vec3 &wishDir, float wishSpeed, float deltaTime);

    void resolveMovementCollisions(float deltaTime);
    void resolveGravityCollisions(float deltaTime);
    std::optional<Capsule::CollisionResolution> getCollisionNormal(SpatialGrid &grid, std::vector<glm::vec3> &vertices);

    void createModelGrid(Scene &scene);
    void createTerrainGrid(Scene &scene);
};
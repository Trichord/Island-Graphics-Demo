#include "character_physics.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

#include "aabb.h"
#include "capsule.h"
#include "glm/glm.hpp"
#include "primitive.h"
#include "scene.h"
#include "shader_types.h"
#include "spatial_grid.h"
#include "world_vectors.h"

constexpr float gridPadding{1.125};

CharacterPhysics::CharacterPhysics(Scene &scene)
    : capsule{scene.getCamera().getPosition(), physicsDefaultValues::playerHeight, physicsDefaultValues::playerWidth},
      resetPosition{scene.getCamera().getPosition()}
{
    constexpr float cellSizeMultiplier{5.0f};

    float worldMinX{std::numeric_limits<float>::infinity()};
    float worldMaxX{-std::numeric_limits<float>::infinity()};

    float worldMinY{std::numeric_limits<float>::infinity()};
    float worldMaxY{-std::numeric_limits<float>::infinity()};

    float worldMinZ{std::numeric_limits<float>::infinity()};
    float worldMaxZ{-std::numeric_limits<float>::infinity()};

    float triangleMaxSpan{-std::numeric_limits<float>::infinity()};
    float triangleMinSpan{std::numeric_limits<float>::infinity()};

    float triangleSum{0.0f};

    for (auto [_, renderables] : scene.getRenderablesByShader())
        for (auto &renderable : renderables)
        {
            using enum CullType;

            if (renderable.properties.cullType == TERRAIN)
            {
                auto &object = renderable.objectRef.get();
                AABB &aabb{object.getAABB()};

                worldMinX = std::min(worldMinX, aabb.getMinX());
                worldMaxX = std::max(worldMaxX, aabb.getMaxX());

                worldMinY = std::min(worldMinY, aabb.getMinY());
                worldMaxY = std::max(worldMaxY, aabb.getMaxY());

                worldMinZ = std::min(worldMinZ, aabb.getMinZ());
                worldMaxZ = std::max(worldMaxZ, aabb.getMaxZ());

                for (auto &mesh : object.getMeshes())
                    for (auto &primitive : mesh.primitives)
                    {
                        auto vertices = primitive.getVertices();
                        auto indices = primitive.getIndices();

                        for (std::uint32_t i{0}; i < indices.size(); i += 3)
                        {
                            glm::vec3 a{vertices[indices[i]].position};
                            glm::vec3 b{vertices[indices[i + 1]].position};
                            glm::vec3 c{vertices[indices[i + 2]].position};

                            float minX{std::min({a.x, b.x, c.x})};
                            float maxX{std::max({a.x, b.x, c.x})};

                            float minZ{std::min({a.z, b.z, c.z})};
                            float maxZ{std::max({a.z, b.z, c.z})};

                            triangleMaxSpan = std::max({triangleMaxSpan, maxX - minX, maxZ - minZ});
                            triangleMinSpan = std::min({triangleMinSpan, maxX - minX, maxZ - minZ});
                            triangleSum += std::max(maxX - minX, maxZ - minZ);

                            meshVertices.push_back(a);
                            meshVertices.push_back(b);
                            meshVertices.push_back(c);
                        }
                    }
            }
        }

    float averageTriangleSize{triangleSum / meshVertices.size() / 3.0f};
    float cellSize{averageTriangleSize * cellSizeMultiplier * gridPadding};

    AABB worldAABB{worldMinX, worldMinY, worldMinZ, worldMaxX, worldMaxY, worldMaxZ};

    terrainGrid = SpatialGrid(std::move(worldAABB), cellSize, meshVertices);
    resetHeight = worldAABB.getMinY();
}

glm::vec3 CharacterPhysics::accelerate(const glm::vec3 &wishDir, float wishSpeed, float accel)
{
    using namespace physicsDefaultValues;

    float currentSpeed{glm::dot(capsule.getVelocity(), wishDir)};
    float addSpeed{wishSpeed - currentSpeed};

    if (addSpeed <= 0)
        return glm::vec3{0.0f};

    float accelSpeed{accel * wishSpeed * surfaceFriction};

    if (accelSpeed > addSpeed)
        accelSpeed = addSpeed;

    return glm::vec3{wishDir * accelSpeed};
}

// Source Engine inspired movement
void CharacterPhysics::update(std::uint8_t direction, const glm::vec3 &front, float deltaTime)
{
    using enum CharacterMovement;
    using namespace WorldVectors;
    using namespace physicsDefaultValues;

    if (capsule.getBottom().y < resetHeight)
    {
        capsule.setPosition(resetPosition);
        capsule.setVelocity(glm::vec3{0.0f});
        previousPosition = resetPosition;
        return;
    }

    previousPosition = capsule.getPosition();

    glm::vec3 flatFront{glm::normalize(glm::vec3{front.x, 0.0f, front.z})};
    glm::vec3 right{glm::cross(flatFront, worldUp)};
    right = glm::normalize(right);

    glm::vec3 wishDir{0.0f};
    glm::vec3 wishVel{0.0f};

    float wishSpeed{0.0f};
    float forwardMove{0.0f};
    float sideMove{0.0f};

    if (direction & FORWARD)
        forwardMove = 1.0f;

    if (direction & BACKWARD)
        forwardMove = -1.0f;

    if (direction & RIGHT)
        sideMove = 1.0f;

    if (direction & LEFT)
        sideMove = -1.0f;

    wishVel = flatFront * forwardMove + right * sideMove;

    wishDir = wishVel;

    if (glm::length(wishVel) > 0.0f)
    {
        wishSpeed = glm::length(wishVel) * playerMaxSpeed;
        wishDir = glm::normalize(wishVel);
    }

    if (wishSpeed > playerMaxSpeed)
    {
        wishVel *= playerMaxSpeed / wishSpeed;
        wishSpeed = playerMaxSpeed;
    }

    float currentSpeed{glm::length(capsule.getVelocity())};

    // Apply drag
    float drag{currentSpeed * surfaceFriction * deltaTime};
    capsule.setVelocity(capsule.getVelocity() * std::max(currentSpeed - drag, 0.0f) / currentSpeed);

    // Stop player if under minimum speed
    if (currentSpeed < playerMinSpeed)
        capsule.setVelocity(glm::vec3{0.0f});

    // Acceleration from wish direction
    glm::vec3 accelerationDir{accelerate(wishDir, playerMaxSpeed, playerMaxAcceleration)};

    capsule.addVelocity(accelerationDir * deltaTime);
    capsule.translate(accelerationDir * deltaTime);

    // Resolve collisions for movement
    glm::vec3 capsuleVelocity{capsule.getVelocity()};
    glm::vec3 horizontalVel{capsuleVelocity.x, 0.0f, capsuleVelocity.z};
    capsule.translate(horizontalVel * deltaTime);
    resolveMovementCollisions();

    // Add in the gravity
    glm::vec3 gravity{0.0f, -gravityForce, 0.0f};
    capsule.addVelocity(gravity * deltaTime);

    // Cap fall speed
    glm::vec3 capVel{capsule.getVelocity()};
    capVel = glm::vec3{capVel.x, std::max(capVel.y, -maxVerticalVelocity), capVel.z};
    capsule.setVelocity(capVel);

    // And resolve any new collisions
    capsule.translate(glm::vec3{0.0f, capsule.getVelocity().y, 0.0f});
    resolveGravityCollisions(deltaTime);

    // Zero out the vertical velocity for the next frame - collision resolutions should have placed us perfectly back on
    // the ground
    glm::vec3 velocity{capsule.getVelocity()};
    capsule.setVelocity(glm::vec3{velocity.x, 0.0f, velocity.z});
}

// Resolve and slide along collision triangle's normal
void CharacterPhysics::resolveMovementCollisions()
{
    using namespace WorldVectors;
    using namespace physicsDefaultValues;

    auto collisionNormal = checkCollisionNormal();

    if (!collisionNormal.has_value())
        return;

    capsule.slide(collisionNormal.value());
}

// Only push capsule up vertically
void CharacterPhysics::resolveGravityCollisions(float deltaTime)
{
    using namespace WorldVectors;
    using namespace physicsDefaultValues;

    auto collisionNormal = checkCollisionNormal();

    if (!collisionNormal.has_value())
        return;

    glm::vec3 normal{collisionNormal.value()};
    glm::vec3 capsuleVelocity{capsule.getVelocity()};

    glm::vec3 verticalPush{0.0f, normal.y, 0.0f};
    capsule.translate(verticalPush);
}

std::optional<glm::vec3> CharacterPhysics::checkCollisionNormal()
{
    using namespace physicsDefaultValues;
    using namespace WorldVectors;
    constexpr float epsilon{0.001f};

    float depth{-std::numeric_limits<float>::infinity()};
    glm::vec3 collisionNormal{0.0f};

    // Get which triangles are close to the capsule
    terrainGrid.query(capsule.getPosition(), triangleCollisions);

    // Find deepest collision
    for (int index : triangleCollisions)
    {
        auto result = capsule.collision(meshVertices[index], meshVertices[index + 1], meshVertices[index + 2]);

        if (result.depth > depth)
        {
            depth = result.depth;
            collisionNormal = result.normal;
        }
    }

    if (depth <= 0.0f)
        return {};

    return collisionNormal * (depth + epsilon);
}

glm::vec3 CharacterPhysics::getPosition() const
{
    return capsule.getPosition();
}

void CharacterPhysics::setPosition(const glm::vec3 &newPosition)
{
    capsule.setPosition(newPosition);
    capsule.setVelocity(glm::vec3{0.0f});
}

const glm::vec3 &CharacterPhysics::getPreviousPosition() const
{
    return previousPosition;
}
#include "player_physics.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <utility>

#include "aabb.h"
#include "capsule.h"
#include "glm/glm.hpp"
#include "primitive.h"
#include "scene.h"
#include "shader_type.h"
#include "spatial_grid.h"
#include "world_vectors.h"

constexpr float modelGridCellSize{50.0f};

PlayerPhysics::PlayerPhysics(Scene &scene)
    : capsule_{scene.getCamera().getPosition(), physicsDefaultValues::playerHeight, physicsDefaultValues::playerWidth},
      resetPosition_{scene.getCamera().getPosition()}
{
    createTerrainGrid(scene);
    createModelGrid(scene);
}

glm::vec3 PlayerPhysics::accelerate(const glm::vec3 &wishDir, float wishSpeed, float accel)
{
    using namespace physicsDefaultValues;

    float currentSpeed{glm::dot(capsule_.getVelocity(), wishDir)};
    float addSpeed{wishSpeed - currentSpeed};

    if (addSpeed <= 0)
        return glm::vec3{0.0f};

    float accelSpeed{accel * wishSpeed * surfaceFriction};

    if (accelSpeed > addSpeed)
        accelSpeed = addSpeed;

    return glm::vec3{wishDir * accelSpeed};
}

// Source Engine inspired movement
void PlayerPhysics::update(std::uint8_t direction, const glm::vec3 &front, float deltaTime)
{
    using namespace physicsDefaultValues;

    movePlayer(direction, front, deltaTime);

    // Resolve collisions for movement
    glm::vec3 capsuleVelocity{capsule_.getVelocity()};
    glm::vec3 horizontalVel{capsuleVelocity.x, 0.0f, capsuleVelocity.z};
    capsule_.translate(horizontalVel * deltaTime);
    resolveMovementCollisions();

    // Add in the gravity
    glm::vec3 gravity{0.0f, -gravityForce, 0.0f};
    capsule_.addVelocity(gravity * deltaTime);

    // Cap fall speed
    glm::vec3 capVel{capsule_.getVelocity()};
    capVel = glm::vec3{capVel.x, std::max(capVel.y, -maxVerticalVelocity), capVel.z};
    capsule_.setVelocity(capVel);

    // And resolve any new collisions
    capsule_.translate(glm::vec3{0.0f, capsule_.getVelocity().y, 0.0f});
    resolveGravityCollisions(deltaTime);

    // Zero out the vertical velocity for the next frame - collision resolutions should have placed us perfectly back on
    // the ground
    glm::vec3 velocity{capsule_.getVelocity()};
    capsule_.setVelocity(glm::vec3{velocity.x, 0.0f, velocity.z});
}

void PlayerPhysics::movePlayer(std::uint8_t direction, const glm::vec3 &front, float deltaTime)
{
    using enum CharacterMovement;
    using namespace WorldVectors;
    using namespace physicsDefaultValues;

    if (capsule_.getBottom().y < resetHeight_)
    {
        capsule_.setPosition(resetPosition_);
        capsule_.setVelocity(glm::vec3{0.0f});
        previousPosition_ = resetPosition_;
        return;
    }

    previousPosition_ = capsule_.getPosition();

    glm::vec3 flatFront{glm::normalize(glm::vec3{front.x, 0.0f, front.z})};
    glm::vec3 right{glm::cross(flatFront, worldUp)};
    right = glm::normalize(right);

    glm::vec3 wishDir{0.0f};
    glm::vec3 wishVel{0.0f};

    float wishSpeed{0.0f};
    float forwardMove{0.0f};
    float sideMove{0.0f};

    if (direction & FORWARD)
        forwardMove += 1.0f;

    if (direction & BACKWARD)
        forwardMove += -1.0f;

    if (direction & RIGHT)
        sideMove += 1.0f;

    if (direction & LEFT)
        sideMove += -1.0f;

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

    float currentSpeed{glm::length(capsule_.getVelocity())};

    // Apply drag
    float drag{currentSpeed * surfaceFriction * deltaTime};
    capsule_.setVelocity(capsule_.getVelocity() * std::max(currentSpeed - drag, 0.0f) / currentSpeed);

    // Stop player if under minimum speed
    if (currentSpeed < playerMinSpeed)
        capsule_.setVelocity(glm::vec3{0.0f});

    // Acceleration from wish direction
    glm::vec3 accelerationDir{accelerate(wishDir, wishSpeed, playerMaxAcceleration)};

    capsule_.addVelocity(accelerationDir * deltaTime);
    capsule_.translate(accelerationDir * deltaTime);
}

// Resolve and slide along collision triangle's normal
void PlayerPhysics::resolveMovementCollisions()
{
    using namespace WorldVectors;
    using namespace physicsDefaultValues;

    // Collisions with models first
    auto collisionNormal = getCollisionNormal(modelGrid_, modelVertices_);

    if (collisionNormal.has_value())
        capsule_.slide(collisionNormal.value());

    // Then ground
    collisionNormal = getCollisionNormal(terrainGrid_, terrainVertices_);

    if (collisionNormal.has_value())
        capsule_.slide(collisionNormal.value());
}

// Only push capsule up vertically
void PlayerPhysics::resolveGravityCollisions(float deltaTime)
{
    using namespace WorldVectors;
    using namespace physicsDefaultValues;

    auto collisionNormal = getCollisionNormal(terrainGrid_, terrainVertices_);

    if (!collisionNormal.has_value())
        return;

    glm::vec3 normal{collisionNormal.value()};
    glm::vec3 capsuleVelocity{capsule_.getVelocity()};

    glm::vec3 verticalPush{0.0f, normal.y, 0.0f};
    capsule_.translate(verticalPush);
}

std::optional<glm::vec3> PlayerPhysics::getCollisionNormal(SpatialGrid &grid, std::vector<glm::vec3> &vertices)
{
    constexpr float epsilon{0.001f};

    float depth{-std::numeric_limits<float>::infinity()};
    glm::vec3 collisionNormal{0.0f};

    // Get which triangles are close to the capsule
    grid.query(capsule_.getPosition(), triangleCollisions_);

    // Find deepest collision
    for (int index : triangleCollisions_)
    {
        auto result = capsule_.collideWithTriangle(vertices[index], vertices[index + 1], vertices[index + 2]);

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

glm::vec3 PlayerPhysics::getPosition() const
{
    return capsule_.getPosition();
}

void PlayerPhysics::setPosition(const glm::vec3 &newPosition)
{
    capsule_.setPosition(newPosition);
    capsule_.setVelocity(glm::vec3{0.0f});
}

const glm::vec3 &PlayerPhysics::getPreviousPosition() const
{
    return previousPosition_;
}

void PlayerPhysics::createModelGrid(Scene &scene)
{
    float worldMinX{std::numeric_limits<float>::infinity()};
    float worldMaxX{-std::numeric_limits<float>::infinity()};

    float worldMinY{std::numeric_limits<float>::infinity()};
    float worldMaxY{-std::numeric_limits<float>::infinity()};

    float worldMinZ{std::numeric_limits<float>::infinity()};
    float worldMaxZ{-std::numeric_limits<float>::infinity()};

    for (auto [_, renderables] : scene.getRenderablesByShader())
        for (auto &renderable : renderables)
        {
            if (renderable.properties.cullMethod == CullMethod::TERRAIN)
                continue;

            auto &object{renderable.objectRef.get()};

            if (!std::holds_alternative<Transform>(renderable.properties.transform))
                continue;

            Transform &transform{std::get<Transform>(renderable.properties.transform)};
            glm::mat4 modelMatrix{transform.getModelMatrix()};

            for (auto &mesh : object.getMeshes())
                for (auto &primitive : mesh.primitives)
                {
                    if (primitive.getAlphaMode() != fastgltf::AlphaMode::Opaque)
                        continue;

                    auto vertices = primitive.getVertices();
                    auto indices = primitive.getIndices();

                    for (std::uint32_t i{0}; i < indices.size(); i += 3)
                    {
                        glm::vec3 a{vertices[indices[i]].position};
                        glm::vec3 b{vertices[indices[i + 1]].position};
                        glm::vec3 c{vertices[indices[i + 2]].position};

                        a = glm::vec3{modelMatrix * glm::vec4{a, 1.0}};
                        b = glm::vec3{modelMatrix * glm::vec4{b, 1.0}};
                        c = glm::vec3{modelMatrix * glm::vec4{c, 1.0}};

                        worldMinX = std::min({worldMinX, a.x, b.x, c.x});
                        worldMaxX = std::max({worldMaxX, a.x, b.x, c.x});

                        worldMinY = std::min({worldMinY, a.y, b.y, c.y});
                        worldMaxY = std::max({worldMaxY, a.y, b.y, c.y});

                        worldMinZ = std::min({worldMinZ, a.z, b.z, c.z});
                        worldMaxZ = std::max({worldMaxZ, a.z, b.z, c.z});

                        float minX{std::min({a.x, b.x, c.x})};
                        float maxX{std::max({a.x, b.x, c.x})};

                        float minZ{std::min({a.z, b.z, c.z})};
                        float maxZ{std::max({a.z, b.z, c.z})};

                        modelVertices_.push_back(a);
                        modelVertices_.push_back(b);
                        modelVertices_.push_back(c);
                    }
                }
        }

    AABB worldAABB{worldMinX, worldMinY, worldMinZ, worldMaxX, worldMaxY, worldMaxZ};

    modelGrid_ = std::move(SpatialGrid(std::move(worldAABB), modelGridCellSize, modelVertices_));
}

void PlayerPhysics::createTerrainGrid(Scene &scene)
{
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
            using enum CullMethod;

            if (renderable.properties.cullMethod == TERRAIN)
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

                            terrainVertices_.push_back(a);
                            terrainVertices_.push_back(b);
                            terrainVertices_.push_back(c);
                        }
                    }
            }
        }

    float averageTriangleSize{triangleSum / terrainVertices_.size() / 3.0f};

    AABB worldAABB{worldMinX, worldMinY, worldMinZ, worldMaxX, worldMaxY, worldMaxZ};

    terrainGrid_ = std::move(SpatialGrid(std::move(worldAABB), averageTriangleSize, terrainVertices_));
    resetHeight_ = worldAABB.getMinY();
}
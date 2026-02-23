#include "character_physics.h"

#include <cstdint>
#include <iostream>
#include <optional>

#include "capsule.h"
#include "glm/glm.hpp"
#include "primitive.h"
#include "spatial_grid.h"
#include "world_vectors.h"

constexpr int maxResolveSteps{3};
constexpr float gridPadding{1.125};
constexpr float groundSnap{0.5f};

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

glm::vec3 CharacterPhysics::accelerate(const glm::vec3 &wishDir, float wishSpeed, float accel, float deltaTime)
{
    using namespace physicsDefaultValues;

    float currentSpeed{glm::dot(capsule.getVelocity(), wishDir)};
    float addSpeed{wishSpeed - currentSpeed};

    if (addSpeed <= 0)
        return glm::vec3{0.0f};

    float accelSpeed{accel * deltaTime * wishSpeed * surfaceFriction};

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
        wishSpeed = glm::length(wishVel) * maxGroundVelocity;
        wishDir = glm::normalize(wishVel);
    }

    if (wishSpeed > playerMaxSpeed)
    {
        wishVel *= playerMaxSpeed / wishSpeed;
        wishSpeed = playerMaxSpeed;
    }

    float currentSpeed{glm::length(capsule.getVelocity())};

    // Apply drag
    if (currentSpeed > 0.0f)
    {
        float drag{currentSpeed * surfaceFriction * deltaTime};
        capsule.setVelocity(capsule.getVelocity() * std::max(currentSpeed - drag, 0.0f) / currentSpeed);
    }

    // Stop player if under minimum speed
    if (currentSpeed < playerMinSpeed)
        capsule.setVelocity(glm::vec3{0.0f});

    // Acceleration from wish direction
    glm::vec3 accelerationDir{accelerate(wishDir, wishSpeed, playerMaxAcceleration, deltaTime)};
    capsule.addVelocity(accelerationDir);
    capsule.translate(accelerationDir * deltaTime);

    // Resolve collisions for movement
    glm::vec3 capsuleVelocity{capsule.getVelocity()};
    glm::vec3 horizontalVel{capsuleVelocity.x, 0.0f, capsuleVelocity.z};
    capsule.translate(horizontalVel * deltaTime);
    resolveMovementCollisions();

    auto closestTriangle{rayCast(capsule.getBottom())};

    if (closestTriangle.distance < groundSnap && glm::dot(closestTriangle.normal, worldUp) > minSlope)
    {
        capsule.translate(glm::vec3{0.0f, -closestTriangle.distance, 0.0f});

        glm::vec3 capVelocity{capsule.getVelocity()};   
        capVelocity = capVelocity - glm::dot(closestTriangle.normal, capVelocity) * closestTriangle.normal;
        capsule.setVelocity(capVelocity * deltaTime);
    }
    else
    {
        glm::vec3 gravity{0.0f, -gravityForce * deltaTime, 0.0f};
        capsule.addVelocity(gravity * deltaTime);

        capsule.translate(glm::vec3{0.0f, capsule.getVelocity().y, 0.0f});
        resolveGravityCollisions(deltaTime);
    }   
}

void CharacterPhysics::resolveMovementCollisions()
{
    using namespace WorldVectors;
    using namespace physicsDefaultValues;

    for (int i{0}; i < maxResolveSteps; ++i)
    {
        auto collisionNormal = checkCollisionNormal();

        if (!collisionNormal.has_value())
            break;

        capsule.slide(collisionNormal.value());
    }
}

void CharacterPhysics::resolveGravityCollisions(float deltaTime)
{
    using namespace WorldVectors;
    using namespace physicsDefaultValues;

    for (int i{0}; i < maxResolveSteps; ++i)
    {
        auto collisionNormal = checkCollisionNormal();

        if (!collisionNormal.has_value())
            break;

        glm::vec3 normal{collisionNormal.value()};
        glm::vec3 capsuleVelocity{capsule.getVelocity()};

        glm::vec3 verticalPush{0.0f, normal.y, 0.0f};
        capsule.translate(verticalPush);
    }
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

// Ray triangle intersection test: https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
CharacterPhysics::RayCast CharacterPhysics::rayCast(const glm::vec3 &position)
{
    constexpr float epsilon = std::numeric_limits<float>::epsilon();
    constexpr glm::vec3 rayVector{0.0f, -1.0f, 0.0f};

    CharacterPhysics::RayCast result;
    float closestDistance{std::numeric_limits<float>::infinity()};

    terrainGrid.query(capsule.getPosition(), triangleCollisions);

    for (int index : triangleCollisions)
    {
        glm::vec3 a{meshVertices[index]};
        glm::vec3 b{meshVertices[index + 1]};
        glm::vec3 c{meshVertices[index + 2]};

        glm::vec3 edge1{b - a};
        glm::vec3 edge2{c - a};
        glm::vec3 rayCrossE2{glm::cross(rayVector, edge2)};
        float det{dot(edge1, rayCrossE2)};

        if (det > -epsilon && det < epsilon)
            continue; // This ray is parallel to this triangle.

        float invDet{1.0f / det};
        glm::vec3 s{position - a};
        float u{invDet * dot(s, rayCrossE2)};

        if ((u < 0 && abs(u) > epsilon) || (u > 1 && abs(u - 1) > epsilon))
            continue;

        glm::vec3 sCrossE1{glm::cross(s, edge1)};
        float v{invDet * glm::dot(rayVector, sCrossE1)};

        if ((v < 0 && abs(v) > epsilon) || (u + v > 1 && abs(u + v - 1) > epsilon))
            continue;

        // At this stage we can compute t to find out where the intersection point is on the line.
        float t{invDet * glm::dot(edge2, sCrossE1)};

        if (t > epsilon && t < closestDistance) // ray intersection
        {
            closestDistance = t;

            result.distance = t;
            result.intersection = glm::vec3{position + rayVector * t};
            result.normal = glm::normalize(glm::cross(edge1, edge2));
        }
    }

    return result;
}
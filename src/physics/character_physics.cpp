#include "character_physics.h"

#include <cstdint>
#include <optional>

#include "capsule.h"
#include "glm/glm.hpp"
#include "primitive.h"
#include "spatial_grid.h"
#include "world_vectors.h"

constexpr float gridPadding{1.125};

CharacterPhysics::CharacterPhysics(Scene &scene)
    : capsule{scene.getCamera().getPosition(), defaultPhysicsValues::playerHeight, defaultPhysicsValues::playerWidth},
      capsulePosition{scene.getCamera().getPosition()}
{
    float worldMinX{std::numeric_limits<float>::infinity()};
    float worldMaxX{-std::numeric_limits<float>::infinity()};

    float worldMinZ{std::numeric_limits<float>::infinity()};
    float worldMaxZ{-std::numeric_limits<float>::infinity()};

    float triangleMaxSpan{-std::numeric_limits<float>::infinity()};

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

                            meshVertices.push_back(a);
                            meshVertices.push_back(b);
                            meshVertices.push_back(c);
                        }
                    }
            }
        }

    float cellSize{triangleMaxSpan * gridPadding};

    AABB worldAABB{worldMinX, 0.0f, worldMinZ, worldMaxX, 0.0f, worldMaxZ};

    terrainGrid = SpatialGrid(std::move(worldAABB), cellSize, meshVertices);
}

void CharacterPhysics::accelerate(glm::vec3 &wishDir, float wishSpeed, float accel, float deltaTime)
{
    using namespace defaultPhysicsValues;

    float currentSpeed{glm::dot(capsuleVelocity, wishDir)};
    float addSpeed{wishSpeed - currentSpeed};

    if (addSpeed <= 0)
        return;

    float accelSpeed{accel * deltaTime * wishSpeed * surfaceFriction};

    if (accelSpeed > addSpeed)
        accelSpeed = addSpeed;

    capsuleVelocity += accelSpeed * wishDir;
}

// Source Engine inspired movement
void CharacterPhysics::update(std::uint8_t direction, const glm::vec3 &front, float deltaTime)
{
    using enum CharacterMovement;
    using namespace WorldVectors;
    using namespace defaultPhysicsValues;

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

    float currentSpeed{glm::length(capsuleVelocity)};

    // Avoid divide by 0
    if (currentSpeed > 0.0f)
    {
        float drag{currentSpeed * surfaceFriction * deltaTime};
        capsuleVelocity *= std::max(currentSpeed - drag, 0.0f) / currentSpeed;
    }

    // Stop player if under minimum speed
    if (currentSpeed < playerMinSpeed)
        capsuleVelocity = glm::vec3{0.0f};

    accelerate(wishDir, wishSpeed, playerMaxAcceleration, deltaTime);

    // Resolve collisions for movement
    capsulePosition += capsuleVelocity * deltaTime;
    resolveCollisions();

    // Resolve collisions for gravity
    capsuleVelocity.y -= gravity * deltaTime;
    capsulePosition += glm::vec3{0.0f, capsuleVelocity.y, 0.0f} * deltaTime;
    resolveCollisions();
}

void CharacterPhysics::resolveCollisions()
{
    constexpr int maxResolveSteps{3};

    capsule.setPosition(capsulePosition);

    for (int i{0}; i < maxResolveSteps; ++i)
    {
        auto collisionNormal = checkCollisionNormal();

        if (!collisionNormal.has_value())
            break;

        capsulePosition += collisionNormal.value();
        capsule.setPosition(capsulePosition);

        slide(collisionNormal.value());
    }
}

void CharacterPhysics::slide(const glm::vec3 &normal)
{
    float velocity_length{glm::length(capsuleVelocity)};
    glm::vec3 velocityNormalized{capsuleVelocity / velocity_length};
    glm::vec3 undesiredMotion = normal * glm::dot(velocityNormalized, normal);
    glm::vec3 desiredMotion = velocityNormalized - undesiredMotion;
    capsuleVelocity = desiredMotion * velocity_length;
}

std::optional<glm::vec3> CharacterPhysics::checkCollisionNormal()
{
    using namespace defaultPhysicsValues;
    using namespace WorldVectors;
    constexpr float epsilon{0.0001f};
    constexpr int maxResolveSteps{5};

    float depth{-std::numeric_limits<float>::infinity()};
    glm::vec3 collisionNormal{0.0f};

    // Get which triangles are close to the capsule
    terrainGrid.query(capsulePosition, triangleCollisions);

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

const glm::vec3 &CharacterPhysics::getPosition() const
{
    return capsulePosition;
}

void CharacterPhysics::setPosition(const glm::vec3 &newPosition)
{
    capsulePosition = newPosition;
    capsuleVelocity = glm::vec3{0.0f};
}
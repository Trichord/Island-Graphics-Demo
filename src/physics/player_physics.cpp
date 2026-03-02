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

constexpr float modelGridCellSize{10.0f};

// Movement
constexpr float groundFriction{5.5f};
constexpr float groundAcceleration{11.0f};
constexpr float gravityForce{10.0f};
constexpr float playerWidth{0.5f};
constexpr float playerHeight{2.0f};
constexpr float playerMaxSpeed{27.0f};
constexpr float playerMinSpeed{0.5f};
constexpr float maxVerticalVelocity{1.5f};

struct WorldInfo
{
    float minX{std::numeric_limits<float>::infinity()};
    float maxX{-std::numeric_limits<float>::infinity()};

    float minY{std::numeric_limits<float>::infinity()};
    float maxY{-std::numeric_limits<float>::infinity()};

    float minZ{std::numeric_limits<float>::infinity()};
    float maxZ{-std::numeric_limits<float>::infinity()};

    float averageTriangleSize{0.0f};
    float triangleSpanSum{0.0f};
    int numTriangles{0};
};

PlayerPhysics::PlayerPhysics(Scene &scene)
    : capsule_{scene.getCamera().getPosition(), playerHeight, playerWidth},
      resetPosition_{scene.getCamera().getPosition()}
{
}

void PlayerPhysics::initializeResources(Scene &scene)
{
    createTerrainGrid(scene);
    createModelGrid(scene);
}

// Source Engine inspired movement
void PlayerPhysics::update(std::uint8_t direction, const glm::vec3 &front, float deltaTime)
{
    movePlayer(direction, front, deltaTime);

    // Resolve collisions for movement
    glm::vec3 capsuleVelocity{capsule_.getVelocity()};
    glm::vec3 horizontalVel{capsuleVelocity.x, 0.0f, capsuleVelocity.z};
    capsule_.translate(horizontalVel * deltaTime);
    resolveMovementCollisions(deltaTime);

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

    // Zero out the vertical velocity for the next frame - collision resolutions should have placed us perfectly
    // back on the ground
    glm::vec3 velocity{capsule_.getVelocity()};
    capsule_.setVelocity(glm::vec3{velocity.x, 0.0f, velocity.z});
}

void PlayerPhysics::movePlayer(std::uint8_t direction, const glm::vec3 &front, float deltaTime)
{
    using enum CharacterMovement;
    using namespace WorldVectors;

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
    float drag{currentSpeed * groundFriction * deltaTime};
    capsule_.setVelocity(capsule_.getVelocity() * std::max(currentSpeed - drag, 0.0f) / currentSpeed);

    // Stop player if under minimum speed
    if (currentSpeed < playerMinSpeed)
        capsule_.setVelocity(glm::vec3{0.0f});

    // Acceleration from wish direction
    glm::vec3 accelerationDir{accelerate(wishDir, wishSpeed, deltaTime)};

    capsule_.addVelocity(accelerationDir * deltaTime);
    capsule_.translate(accelerationDir * deltaTime);
}

glm::vec3 PlayerPhysics::accelerate(const glm::vec3 &wishDir, float wishSpeed, float deltaTime)
{

    float currentSpeed{glm::dot(capsule_.getVelocity(), wishDir)};
    float addSpeed{wishSpeed - currentSpeed};

    if (addSpeed <= 0)
        return glm::vec3{0.0f};

    float accelSpeed{deltaTime * wishSpeed * groundAcceleration};

    if (accelSpeed > addSpeed)
        accelSpeed = addSpeed;

    return glm::vec3{wishDir * accelSpeed};
}

// Resolve and slide along collision triangle's normal
void PlayerPhysics::resolveMovementCollisions(float deltaTime)
{
    using namespace WorldVectors;

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

    auto collisionNormal = getCollisionNormal(modelGrid_, modelVertices_);
    if (collisionNormal.has_value())
    {
        auto collision{collisionNormal.value()};
        glm::vec3 capsuleVelocity{capsule_.getVelocity()};

        glm::vec3 verticalPush{0.0f, collision.normal.y, 0.0f};
        capsule_.translate(verticalPush * collision.depth);
    }

    collisionNormal = getCollisionNormal(terrainGrid_, terrainVertices_);
    if (!collisionNormal.has_value())
        return;

    auto collision{collisionNormal.value()};

    glm::vec3 verticalPush{0.0f, collision.normal.y, 0.0f};
    capsule_.translate(verticalPush * collision.depth);
}

std::optional<Capsule::CollisionResolution> PlayerPhysics::getCollisionNormal(SpatialGrid &grid,
                                                                              std::vector<glm::vec3> &vertices)
{
    constexpr float epsilon{0.001f};

    float maxDepth{-std::numeric_limits<float>::infinity()};
    Capsule::CollisionResolution result;

    // Get which triangles are close to the capsule
    grid.query(capsule_.getPosition(), collisionTriangles_);

    // Find deepest collision
    for (int index : collisionTriangles_)
    {
        auto collision = capsule_.collideWithTriangle(vertices[index], vertices[index + 1], vertices[index + 2]);

        if (collision.depth > maxDepth)
        {
            maxDepth = collision.depth;
            result = collision;
        }
    }

    if (maxDepth <= 0.0f)
        return {};

    result.depth += epsilon;
    result.normal = glm::normalize(result.normal);

    return result;
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

void iterateVertices(std::vector<glm::vec3> &target, const std::span<Primitive::Mesh> &meshes, WorldInfo &worldInfo,
                     const glm::mat4 &modelMatrix)
{
    for (auto &mesh : meshes)
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

                // AABB
                worldInfo.minX = std::min({worldInfo.minX, a.x, b.x, c.x});
                worldInfo.maxX = std::max({worldInfo.maxX, a.x, b.x, c.x});

                worldInfo.minY = std::min({worldInfo.minY, a.y, b.y, c.y});
                worldInfo.maxY = std::max({worldInfo.maxY, a.y, b.y, c.y});

                worldInfo.minZ = std::min({worldInfo.minZ, a.z, b.z, c.z});
                worldInfo.maxZ = std::max({worldInfo.maxZ, a.z, b.z, c.z});

                // Add local triangle to the total span
                float minX{std::min({a.x, b.x, c.x})};
                float maxX{std::max({a.x, b.x, c.x})};

                float minZ{std::min({a.z, b.z, c.z})};
                float maxZ{std::max({a.z, b.z, c.z})};

                worldInfo.triangleSpanSum += (maxX - minX + maxZ - minZ) * 0.5f;

                target.push_back(a);
                target.push_back(b);
                target.push_back(c);
            }

            worldInfo.numTriangles += static_cast<int>(indices.size() / 3);
        }
}

WorldInfo traverseSceneGraph(Scene &scene, std::vector<glm::vec3> &vertices, bool terrain)
{
    WorldInfo worldInfo;

    for (auto [_, renderables] : scene.getRenderablesByShader())
        for (auto &renderable : renderables)
        {
            bool isTerrain{renderable.properties.cullMethod == CullMethod::TERRAIN};

            if (isTerrain != terrain)
                continue;

            auto &object = renderable.objectRef.get();

            if (terrain)
            {
                auto meshes = object.getMeshes();
                Transform &transform{std::get<Transform>(renderable.properties.transform)};
                iterateVertices(vertices, meshes, worldInfo, transform.getModelMatrix());
            }
            else
            {
                auto meshes = object.loadCollisionMesh();

                if (!meshes.has_value())
                    continue;

                if (std::holds_alternative<Transform>(renderable.properties.transform))
                {
                    Transform &transform{std::get<Transform>(renderable.properties.transform)};
                    iterateVertices(vertices, meshes.value(), worldInfo, transform.getModelMatrix());
                }
                else
                {
                    auto &transform{std::get<std::vector<InstancedData>>(renderable.properties.transform)};
                    for (InstancedData &data : transform)
                        iterateVertices(vertices, meshes.value(), worldInfo, data.modelMatrix);
                }
            }
        }

    worldInfo.averageTriangleSize = worldInfo.triangleSpanSum / worldInfo.numTriangles;

    return worldInfo;
}

void PlayerPhysics::createModelGrid(Scene &scene)
{
    WorldInfo worldInfo{traverseSceneGraph(scene, modelVertices_, false)};

    AABB worldAABB{worldInfo.minX, worldInfo.minY, worldInfo.minZ, worldInfo.maxX, worldInfo.maxY, worldInfo.maxZ};

    modelGrid_ = std::move(SpatialGrid(std::move(worldAABB), modelGridCellSize, modelVertices_));
}

void PlayerPhysics::createTerrainGrid(Scene &scene)
{
    WorldInfo worldInfo{traverseSceneGraph(scene, terrainVertices_, true)};

    AABB worldAABB{worldInfo.minX, worldInfo.minY, worldInfo.minZ, worldInfo.maxX, worldInfo.maxY, worldInfo.maxZ};

    terrainGrid_ = std::move(SpatialGrid(std::move(worldAABB), worldInfo.averageTriangleSize, terrainVertices_));
    resetHeight_ = worldInfo.minY;
}
#include "character_physics.h"

#include <cstdint>

#include "capsule.h"
#include "glm/glm.hpp"
#include "primitive.h"
#include "spatial_grid.h"
#include "world_vectors.h"

constexpr float gridPadding{1.125};

CharacterPhysics::CharacterPhysics(Scene &scene)
    : capsule{scene.getCamera().getPosition(), defaultPhysicsValues::playerHeight, defaultPhysicsValues::playerWidth}
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

void CharacterPhysics::update(CharacterMovement direction, const glm::vec3 &front, float deltaTime)
{
    constexpr float epsilon{0.0001f};
    constexpr int maxResolveSteps{5};

    using enum CharacterMovement;
    using namespace WorldVectors;
    using namespace defaultPhysicsValues;

    glm::vec3 acceleration{0.0f};

    switch (direction)
    {
    case FORWARD:
        acceleration += front * playerMovementSpeed;
        break;

    case BACKWARD:
        acceleration -= front * playerMovementSpeed;
        break;

    case LEFT: {
        glm::vec3 right{glm::cross(front, worldUp)};
        right = glm::normalize(right);

        acceleration -= right * playerMovementSpeed;
        break;
    }

    case RIGHT: {
        glm::vec3 right{glm::cross(front, worldUp)};
        right = glm::normalize(right);

        acceleration += right * playerMovementSpeed;
        break;
    }

    case NONE:
        break;
    }

    // Apply acceleration to the position
    acceleration.y -= gravity;
    velocity += acceleration * deltaTime;
    velocity *= (1.0 - drag * deltaTime);

    if (glm::length(velocity) > maxVelocity)
        velocity = glm::normalize(velocity) * maxVelocity;

    position += velocity * deltaTime;
    capsule.setPosition(position);

    // Run collision check multiple times to avoid being pushed through nearby triangles
    for (int i{0}; i < maxResolveSteps; ++i)
    {
        // Get which triangles are close to the capsule
        terrainGrid.query(position, triangleCollisions);

        float depth{-std::numeric_limits<float>::infinity()};
        glm::vec3 collisionNormal{0.0f};

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
            break;

        // Slide. Credit Wicked Engine: https://wickedengine.net/2020/04/capsule-collision-detection/

        if (depth > 0.0f)
        {
            float velocity_length{glm::length(velocity)};
            glm::vec3 velocityNormalized{velocity / velocity_length};
            glm::vec3 undesiredMotion = collisionNormal * glm::dot(velocityNormalized, collisionNormal);
            glm::vec3 desiredMotion = velocityNormalized - undesiredMotion;
            velocity = desiredMotion * velocity_length;

            position += collisionNormal * (depth + epsilon);
            capsule.setPosition(position);
        }
    }
}

const glm::vec3 &CharacterPhysics::getPosition() const
{
    return position;
}

void CharacterPhysics::setPosition(const glm::vec3 &newPosition)
{
    position = newPosition;
    velocity = glm::vec3{0.0f};
}
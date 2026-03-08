#include "player_physics.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <variant>
#include <vector>

#include "jolt/jolt.h"

#include "cull_method.h"
#include "glm/glm.hpp"
#include "instance_data.h"
#include "jolt/Core/Core.h"
#include "jolt/Core/Factory.h"
#include "jolt/Core/TempAllocator.h"
#include "jolt/Geometry/Triangle.h"
#include "jolt/Math/Quat.h"
#include "jolt/Math/Real.h"
#include "jolt/Math/Vec3.h"
#include "jolt/Physics/Body/BodyCreationSettings.h"
#include "jolt/Physics/Body/MotionType.h"
#include "jolt/Physics/Collision/Shape/MeshShape.h"
#include "jolt/Physics/Collision/Shape/Shape.h"
#include "jolt/Physics/EActivation.h"
#include "jolt/Physics/PhysicsSystem.h"
#include "jolt_impl.h"
#include "primitive.h"
#include "renderable.h"
#include "scene.h"
#include "transform.h"
#include "world_vectors.h"

void iterateVertices(JPH::TriangleList &triangles, const std::span<Primitive::Mesh> &meshes,
                     const glm::mat4 &modelMatrix, float &lowestPoint);

// Physics engine
constexpr JPH::uint maxBodies{1024};
constexpr JPH::uint maxBodyPairs{1024};
constexpr JPH::uint maxContactConstraints{1024};

PlayerPhysics::PlayerPhysics(Scene &scene, JPH::PhysicsSystem &physicsSystem, JPH::TempAllocatorImpl &tempAllocator)
    : resetPosition_{toJPHVec3(scene.getCamera().getPosition())}, physicsSystem_{physicsSystem},
      tempAllocator_{tempAllocator}
{
    using namespace JPH;

    physicsSystem_.Init(maxBodies, 0, maxBodyPairs, maxContactConstraints, broadPhaseLayer_,
                        objectVsBroadPhaseLayerFilter_, objectLayerPairFilter_);

    bodyInterface_ = &physicsSystem_.GetBodyInterfaceNoLock();

    player_.init(physicsSystem, tempAllocator);

    // Create terrain and flag it to survive this constructor call
    JPH::TriangleList terrain{traverseSceneGraph(scene)};
    MeshShapeSettings terrainShapeSettings{terrain};
    terrainShapeSettings.SetEmbedded();

    // Create the shape
    ShapeSettings::ShapeResult terrainResult{terrainShapeSettings.Create()};
    ShapeRefC terrainShape{terrainResult.Get()};

    // Terrain properties
    BodyCreationSettings terrainSettings{terrainShape, RVec3::sZero(), Quat::sIdentity(), EMotionType::Static,
                                         Layers::NON_MOVING};

    // Create the actual rigid body
    terrain_ = bodyInterface_->CreateBody(terrainSettings);

    // Add it to the world
    bodyInterface_->AddBody(terrain_->GetID(), EActivation::DontActivate);
    physicsSystem_.OptimizeBroadPhase();

    // Set start position
    glm::vec3 cameraPos{scene.getCamera().getPosition()};
    player_.setPosition(JPH::Vec3{cameraPos.x, cameraPos.y, cameraPos.z});
}

PlayerPhysics::~PlayerPhysics()
{
    using namespace JPH;

    bodyInterface_->RemoveBody(terrain_->GetID());
    bodyInterface_->DestroyBody(terrain_->GetID());
}

void PlayerPhysics::update(std::uint8_t direction, const glm::vec3 &front, bool jump, bool crouch, float deltaTime)
{
    // Player has fallen through the world
    if (player_.getPosition().GetY() < resetHeight_)
    {
        player_.setPosition(resetPosition_);
        previousPosition_ = toGLM(resetPosition_);
        player_.setVelocity(JPH::Vec3{0.0f, 0.0f, 0.0f});
        return;
    }

    previousPosition_ = toGLM(player_.getCenter());
    movePlayer(direction, front, jump, crouch, deltaTime);
}

void PlayerPhysics::movePlayer(std::uint8_t direction, const glm::vec3 &front, bool jump, bool crouch, float deltaTime)
{
    using enum CharacterMovement;
    using namespace WorldVectors;

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

    glm::vec3 forward{glm::normalize(glm::vec3{front.x, 0.0f, front.z})};
    glm::vec3 right{glm::cross(forward, worldUp)};
    glm::vec3 wishDir{forward * forwardMove + right * sideMove};

    player_.handleInput(toJPHVec3(wishDir), jump, deltaTime);
    player_.physicsUpdate(deltaTime);
}

glm::vec3 PlayerPhysics::getPosition() const
{
    return toGLM(player_.getCenter());
}

void PlayerPhysics::setPosition(const glm::vec3 &newPosition)
{
    player_.setPosition(toJPHVec3(newPosition));
}

const glm::vec3 &PlayerPhysics::getPreviousPosition() const
{
    return previousPosition_;
}

void PlayerPhysics::toggleCrouch(bool enable)
{
    player_.toggleCrouch(enable);
}

void PlayerPhysics::toggleSprint(bool enable)
{
    player_.toggleSprint(enable);
}

void iterateVertices(JPH::TriangleList &triangles, const std::span<Primitive::Mesh> &meshes,
                     const glm::mat4 &modelMatrix, float &lowestPoint)
{
    for (auto &mesh : meshes)
        for (auto &primitive : mesh.primitives)
        {
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

                lowestPoint = std::min({lowestPoint, a.y, b.y, c.y});

                triangles.emplace_back(toJPHFloat3(a), toJPHFloat3(b), toJPHFloat3(c));
            }
        }
}

JPH::TriangleList PlayerPhysics::traverseSceneGraph(Scene &scene)
{
    using enum CullMethod;

    JPH::TriangleList vertices;
    float lowestPoint{std::numeric_limits<float>::infinity()};

    for (auto [_, renderables] : scene.getRenderablesByShader())
        for (auto &renderable : renderables)
        {
            auto &object = renderable.objectRef.get();

            // Always use the terrain's actual mesh
            if (renderable.properties.cullMethod == TERRAIN)
            {
                auto meshes = object.getMeshes();
                Transform &transform{std::get<Transform>(renderable.properties.transform)};
                iterateVertices(vertices, meshes, transform.getModelMatrix(), lowestPoint);
            }
            // Otherwise, use the collision mesh if it exists
            else
            {
                auto meshes = object.loadCollisionMesh();

                if (!meshes.has_value())
                    continue;

                // Single instance
                if (std::holds_alternative<Transform>(renderable.properties.transform))
                {
                    Transform &transform{std::get<Transform>(renderable.properties.transform)};
                    iterateVertices(vertices, meshes.value(), transform.getModelMatrix(), lowestPoint);
                }
                // Instanced model
                else
                {
                    auto &transform{std::get<std::vector<InstancedData>>(renderable.properties.transform)};
                    for (InstancedData &data : transform)
                        iterateVertices(vertices, meshes.value(), data.modelMatrix, lowestPoint);
                }
            }
        }

    resetHeight_ = lowestPoint;

    return vertices;
}
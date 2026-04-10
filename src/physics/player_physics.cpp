#include "player_physics.h"

#include <cstdint>

#include "jolt/jolt.h"

#include "glm/glm.hpp"
#include "jolt/Core/TempAllocator.h"
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
#include "physics_utilities.h"
#include "scene.h"
#include "world_vectors.h"

PlayerPhysics::PlayerPhysics(Scene &scene, JPH::PhysicsSystem &physicsSystem, JPH::TempAllocatorImpl &tempAllocator)
    : resetPosition_{toJPHVec3(scene.getCamera().getPosition())}, physicsSystem_{physicsSystem},
      tempAllocator_{tempAllocator}
{
    using namespace JPH;

    bodyInterface_ = &physicsSystem_.GetBodyInterfaceNoLock();

    player_.init(physicsSystem, tempAllocator);

    // Build the static collision mesh from scene
    physicsUtilities::CollisionMesh collisionMesh{physicsUtilities::buildCollisionMesh(scene)};
    resetHeight_ = collisionMesh.minimumHeight;

    // Create terrain and flag it to survive this constructor call
    MeshShapeSettings terrainShapeSettings{collisionMesh.mesh};
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
    constexpr float maxFallVelocity{-30.0f};

    // Player has fallen through the world
    if (player_.getPosition().GetY() < resetHeight_)
    {
        player_.setPosition(resetPosition_);
        previousPosition_ = toGLM(resetPosition_);

        JPH::Vec3 velocity{player_.getVelocity()};
        JPH::Vec3 newVelocity{velocity.GetX(), std::max(velocity.GetY(), maxFallVelocity), velocity.GetZ()};
        player_.setVelocity(newVelocity);

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
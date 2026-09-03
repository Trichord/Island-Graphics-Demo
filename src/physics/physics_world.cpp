#include "physics/physics_world.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Core.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Body/BodyType.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <utility>

#include "application/input.h"
#include "physics/character_capsule.h"
#include "physics/jolt_impl.h"
#include "physics/physics_utilities.h"
#include "scene/scene.h"
#include "utilities/transform.h"
#include "utilities/world_vectors.h"

namespace
{
constexpr CharacterCapsule::Settings capsuleSettings{
    .radius = 0.3f,
    .standingHeight = 1.35f,
    .crouchingHeight = 0.8f,
    .innerShapeFraction = 0.9f,
    .speed = 3.5f,
    .jumpSpeed = 2.75f,
    .sprintSpeedMultiplier = 1.45f,
    .crouchSpeedMultiplier = 0.4f,
};

bool initializePhysicsSystem(JPH::PhysicsSystem &physicsSystem, BPLayerInterfaceImpl &broadPhase,
                             ObjectVsBroadPhaseLayerFilterImpl &bfFilter, ObjectLayerPairFilterImpl &pairFilter,
                             JPH::TempAllocatorImpl &tempAllocator)
{
    // Physics engine constants
    constexpr JPH::uint maxBodies{1024};
    constexpr JPH::uint maxBodyPairs{1024};
    constexpr JPH::uint maxContactConstraints{1024};

    physicsSystem.Init(maxBodies, 0, maxBodyPairs, maxContactConstraints, broadPhase, bfFilter, pairFilter);

    return true;
}

} // namespace

PhysicsWorld::PhysicsWorld()
    : initialized_{initializePhysicsSystem(physicsSystem_, broadPhaseLayer_, objectVsBroadPhaseLayerFilter_,
                                           objectLayerPairFilter_, tempAllocator_)},
      player_{physicsSystem_, tempAllocator_, capsuleSettings, playerID_}
{
}

PhysicsWorld::~PhysicsWorld()
{
    using namespace JPH;

    BodyInterface &interface{physicsSystem_.GetBodyInterface()};

    for (BodyID &id : bodyIDs_)
    {
        interface.RemoveBody(id);
        interface.DestroyBody(id);
    }
}

void PhysicsWorld::initializeScene(Scene &scene)
{
    const glm::vec3 &startingPosition{scene.getCameraDescription().position};

    physics::StaticBodies collisionMeshes{physics::buildCollisionGeometry(scene, physicsSystem_.GetBodyInterface())};
    resetHeight_ = collisionMeshes.minimumHeight;
    resetPosition_ = toJPHVec3(startingPosition);
    player_.setCenter(toJPHVec3(startingPosition));
    bodyIDs_ = std::move(collisionMeshes.ids);
    physicsSystem_.OptimizeBroadPhase();
}

void PhysicsWorld::update(const Input &input, const glm::vec3 &front, float deltaTime)
{
    using namespace JPH;

    constexpr int lookAheadTicks{4};
    constexpr float impactTrigger{4.5f};
    constexpr float jumpDelay{0.7f};

    // Avoid feedback loop when framerate drops
    timeAccumulator_ = std::min(timeAccumulator_ + deltaTime, maxAccumulator_);
    jumpAccumulator_ += deltaTime;
    const bool jump{input.jump && jumpAccumulator_ > jumpDelay};

    player_.toggleCrouch(input.crouchToggle);
    player_.toggleSprint(input.sprintToggle);

    while (timeAccumulator_ > physicsTick_)
    {
        const bool wasSupported{player_.isSupported()};
        const Vec3 preVelocity{player_.getVelocity()};

        previousPosition_ = player_.getCenter();
        tick(input.direction, front, jump && wasSupported, physicsTick_);
        flagMovedEntities();
        timeAccumulator_ -= physicsTick_;

        // Ignore footstep accumulator on steep slopes
        if (player_.isGrounded())
            footstepAccumulator_ += (player_.getCenter() - previousPosition_).Length();

        // Skip the prediction if we already detected a landing
        const std::optional<float> impactSpeed{
            landingTriggered_ ? std::nullopt : player_.predictImpactSpeed(lookAheadTicks * physicsTick_)};

        if (impactSpeed && impactSpeed.value() >= impactTrigger)
        {
            // Player is hitting hard enough to trigger a landing event, reset footsteps
            footstepAccumulator_ = 0.0f;
            landingTriggered_ = true;
        }

        if (jump && wasSupported && !player_.isSupported())
        {
            jumpTriggered_ = true;
            jumpAccumulator_ = 0.0f;
        }
    }

    interpolationAlpha_ = timeAccumulator_ / physicsTick_;
}

void PhysicsWorld::tick(std::uint8_t direction, const glm::vec3 &front, bool jump, float deltaTime)
{
    using namespace JPH;

    constexpr float maxFallVelocity{-30.0f};
    constexpr float resetHeight{5.0f};

    // Player has fallen through the world
    if (player_.getPosition().GetY() < resetHeight_)
    {
        player_.setCenter(resetPosition_ + Vec3{0.0f, resetHeight, 0.0f});

        // Override last position too, so we don't interpolate between two wildly different points
        previousPosition_ = player_.getCenter();

        Vec3 velocity{player_.getVelocity()};
        Vec3 newVelocity{velocity.GetX(), std::max(velocity.GetY(), maxFallVelocity), velocity.GetZ()};
        player_.setVelocity(newVelocity);

        footstepAccumulator_ = 0.0f;
        jumpAccumulator_ = 0.0f;
        landingTriggered_ = false;
        jumpTriggered_ = false;

        return;
    }

    movePlayer(direction, front, jump, deltaTime);
    physicsSystem_.Update(deltaTime, 1, &tempAllocator_, &jobSystem_);
}

void PhysicsWorld::movePlayer(std::uint8_t direction, const glm::vec3 &front, bool jump, float deltaTime)
{
    using enum Input::Direction;
    using namespace WorldVectors;

    float forwardMove{0.0f};
    float sideMove{0.0f};

    if (direction & forward)
        forwardMove += 1.0f;

    if (direction & backward)
        forwardMove -= 1.0f;

    if (direction & right)
        sideMove += 1.0f;

    if (direction & left)
        sideMove -= 1.0f;

    glm::vec3 flatForward{glm::normalize(glm::vec3{front.x, 0.0f, front.z})};
    glm::vec3 rightDir{glm::cross(flatForward, worldUp)};
    JPH::Vec3 wishDir{toJPHVec3(glm::vec3{flatForward * forwardMove + rightDir * sideMove})};

    if (wishDir.LengthSq())
        wishDir = wishDir.Normalized();

    player_.handleInput(wishDir, jump, deltaTime);
    player_.physicsUpdate(deltaTime);
}

bool PhysicsWorld::playerIsGrounded() const
{
    return player_.isGrounded();
}

bool PhysicsWorld::playerIsSupported() const
{
    return player_.isSupported();
}

glm::vec3 PhysicsWorld::getPlayerVelocity() const
{
    return toGLMVec3(player_.getVelocity());
}

void PhysicsWorld::setPlayerPosition(const glm::vec3 &newPosition)
{
    player_.setPosition(toJPHVec3(newPosition));
}

glm::vec3 PhysicsWorld::getPlayerPosition() const
{
    // Interpolate camera between last position and latest physics tick (Glenn Fiedler)
    return glm::mix(toGLMVec3(previousPosition_), toGLMVec3(player_.getCenter()), interpolationAlpha_);
};

bool PhysicsWorld::footstepEvent()
{
    if (footstepAccumulator_ >= footstepDistance_)
    {
        footstepAccumulator_ = std::fmod(footstepAccumulator_, footstepDistance_);
        return true;
    }

    return false;
}

bool PhysicsWorld::jumpEvent()
{
    return std::exchange(jumpTriggered_, false);
}

bool PhysicsWorld::landingEvent()
{
    return std::exchange(landingTriggered_, false);
}

void PhysicsWorld::flagMovedEntities()
{
    using namespace JPH;

    physicsSystem_.GetActiveBodies(EBodyType::RigidBody, activeBodies_);
    const BodyLockInterfaceNoLock &lockInterface{physicsSystem_.GetBodyLockInterfaceNoLock()};
    const BodyInterface &interface{physicsSystem_.GetBodyInterface()};

    for (const BodyID &id : activeBodies_)
    {
        uint64 userData{interface.GetUserData(id)};

        if (userData == playerID_)
            continue;

        movedEntities_.emplace(id);
    }
}

void PhysicsWorld::synchronizeEntries(Scene &scene)
{
    using namespace JPH;

    const BodyInterface &interface{physicsSystem_.GetBodyInterface()};

    for (const JPH::BodyID &id : movedEntities_)
    {
        const RVec3 pos{interface.GetPosition(id)};
        const Quat rot{interface.GetRotation(id)};
        physics::BodyIdentifier sceneBody{physics::getBodyIdentifier(interface.GetUserData(id))};

        Scene::Entry &entry{scene.editEntry(sceneBody.handle)};
        Transform &transform{entry.worldTransforms[sceneBody.transformIndex]};
        transform.setAbsoluteRotation(toGLMQuat(rot));
        transform.setAbsoluteTranslation(toGLMVec3(pos));
    }

    movedEntities_.clear();
}
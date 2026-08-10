#include "physics/player_physics.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include <utility>

#include "physics/character_capsule.h"
#include "physics/jolt_impl.h"
#include "physics/physics_utilities.h"
#include "scene/scene.h"
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
}

PlayerPhysics::PlayerPhysics(JPH::PhysicsSystem &physicsSystem, JPH::TempAllocatorImpl &tempAllocator, Scene &scene,
                             const glm::vec3 &startingPosition)
    : resetPosition_{toJPHVec3(startingPosition)}, physicsSystem_{physicsSystem}, tempAllocator_{tempAllocator},
      bodyInterface_{&physicsSystem.GetBodyInterface()}, player_{physicsSystem, tempAllocator, capsuleSettings}
{
    physics::StaticBodies collisionMeshes{physics::buildCollisionGeometry(scene, physicsSystem.GetBodyInterface())};
    resetHeight_ = collisionMeshes.minimumHeight;
    bodyIDs_ = std::move(collisionMeshes.ids);
    physicsSystem_.OptimizeBroadPhase();
    player_.setCenter(toJPHVec3(startingPosition));
}

PlayerPhysics::~PlayerPhysics()
{
    using namespace JPH;

    for (JPH::BodyID &id : bodyIDs_)
    {
        bodyInterface_->RemoveBody(id);
        bodyInterface_->DestroyBody(id);
    }
}

void PlayerPhysics::update(std::uint8_t direction, const glm::vec3 &front, float deltaTime)
{
    using namespace JPH;

    constexpr int lookAheadTicks{4};
    constexpr float impactTrigger{4.5f};

    // Avoid feedback loop when framerate drops
    timeAccumulator_ = std::min(timeAccumulator_ + deltaTime, maxAccumulator_);
    const bool jump{(direction & CharacterMovement::UP) != 0};

    while (timeAccumulator_ > physicsTick_)
    {
        const bool wasSupported{player_.isSupported()};
        const Vec3 preVelocity{player_.getVelocity()};

        previousPosition_ = player_.getCenter();
        tick(direction, front, jump, physicsTick_);
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
            jumpTriggered_ = true;
    }
}

void PlayerPhysics::tick(std::uint8_t direction, const glm::vec3 &front, bool jump, float deltaTime)
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
        landingTriggered_ = false;
        jumpTriggered_ = false;

        return;
    }

    movePlayer(direction, front, jump, deltaTime);
}

void PlayerPhysics::movePlayer(std::uint8_t direction, const glm::vec3 &front, bool jump, float deltaTime)
{
    using enum CharacterMovement;
    using namespace WorldVectors;

    float forwardMove{0.0f};
    float sideMove{0.0f};

    if (direction & FORWARD)
        forwardMove += 1.0f;

    if (direction & BACKWARD)
        forwardMove -= 1.0f;

    if (direction & RIGHT)
        sideMove += 1.0f;

    if (direction & LEFT)
        sideMove -= 1.0f;

    glm::vec3 forward{glm::normalize(glm::vec3{front.x, 0.0f, front.z})};
    glm::vec3 right{glm::cross(forward, worldUp)};
    JPH::Vec3 wishDir{toJPHVec3(glm::vec3{forward * forwardMove + right * sideMove})};

    if (wishDir.LengthSq())
        wishDir = wishDir.Normalized();

    player_.handleInput(wishDir, jump, deltaTime);
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

glm::vec3 PlayerPhysics::getInterpolatedPosition() const
{
    // Interpolate camera between last position and latest physics tick (Glenn Fiedler)
    float alpha{timeAccumulator_ / physicsTick_};
    return glm::mix(toGLM(previousPosition_), getPosition(), alpha);
};

void PlayerPhysics::toggleCrouch(bool enable)
{
    player_.toggleCrouch(enable);
}

void PlayerPhysics::toggleSprint(bool enable)
{
    player_.toggleSprint(enable);
}

bool PlayerPhysics::footstepEvent()
{
    if (footstepAccumulator_ >= footstepDistance_)
    {
        footstepAccumulator_ = std::fmod(footstepAccumulator_, footstepDistance_);
        return true;
    }

    return false;
}

bool PlayerPhysics::jumpEvent()
{
    return std::exchange(jumpTriggered_, false);
}

bool PlayerPhysics::landingEvent()
{
    return std::exchange(landingTriggered_, false);
}
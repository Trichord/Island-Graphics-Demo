#include "physics/player_physics.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include <utility>

#include "physics/jolt_impl.h"
#include "physics/physics_utilities.h"
#include "scene/scene.h"
#include "utilities/world_vectors.h"

PlayerPhysics::PlayerPhysics(const Scene &scene, JPH::PhysicsSystem &physicsSystem,
                             JPH::TempAllocatorImpl &tempAllocator)
    : resetPosition_{toJPHVec3(scene.getCamera().getPosition())}, physicsSystem_{physicsSystem},
      tempAllocator_{tempAllocator}, bodyInterface_{&physicsSystem.GetBodyInterface()},
      player_{physicsSystem, tempAllocator}
{
    using namespace JPH;

    physics::StaticBodies staticBodies{physics::buildCollisionGeometry(scene, *bodyInterface_)};
    resetHeight_ = staticBodies.minimumHeight;
    bodyIDs_ = std::move(staticBodies.ids);
    physicsSystem_.OptimizeBroadPhase();

    // Set start position
    glm::vec3 cameraPos{scene.getCamera().getPosition()};
    player_.setCenter(JPH::Vec3{cameraPos.x, cameraPos.y, cameraPos.z});
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
    // Avoid feedback loop when framerate drops
    timeAccumulator_ = std::min(timeAccumulator_ + deltaTime, maxAccumulator_);
    const bool jump{(direction & CharacterMovement::UP) != 0};
    const bool wasGrounded{player_.isGrounded()};

    while (timeAccumulator_ > physicsTick_)
    {
        previousPosition_ = player_.getCenter();
        tick(direction, front, jump, physicsTick_);
        timeAccumulator_ -= physicsTick_;

        if (player_.isGrounded())
            footstepAccumulator_ += (player_.getCenter() - previousPosition_).Length();
    }

    // Player just left the ground, queue up a landing event flag and reset footstep tracking
    if (wasGrounded && !player_.isGrounded())
    {
        footstepAccumulator_ = 0.0f;
        landingTriggered_ = false;
    }
}

void PlayerPhysics::tick(std::uint8_t direction, const glm::vec3 &front, bool jump, float deltaTime)
{
    constexpr float maxFallVelocity{-30.0f};

    // Player has fallen through the world
    if (player_.getPosition().GetY() < resetHeight_)
    {
        player_.setCenter(resetPosition_);

        // Override last position too, so we don't interpolate between two wildly different points
        previousPosition_ = player_.getCenter();

        JPH::Vec3 velocity{player_.getVelocity()};
        JPH::Vec3 newVelocity{velocity.GetX(), std::max(velocity.GetY(), maxFallVelocity), velocity.GetZ()};
        player_.setVelocity(newVelocity);

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
    return jumpTriggered_;
}

bool PlayerPhysics::landingEvent()
{
    using namespace JPH;

    constexpr float earlyTriggerTime{0.2f};
    constexpr float fallSpeedTrigger{-3.0f};
    static const SphereShape detectionShape{player_.getRadius()};

    if (landingTriggered_)
        return false;

    const Vec3 velocity{player_.getVelocity()};

    if (velocity.GetY() > fallSpeedTrigger)
        return false;

    const Vec3 direction{velocity * earlyTriggerTime};
    const RShapeCast shapeCast{&detectionShape, Vec3::sReplicate(1.0f), RMat44::sTranslation(player_.getPosition()),
                               direction};

    ShapeCastSettings settings;
    ClosestHitCollisionCollector<CastShapeCollector> collector;
    physicsSystem_.GetNarrowPhaseQuery().CastShape(shapeCast, settings, player_.getPosition(), collector);

    if (!collector.HadHit())
        return false;

    landingTriggered_ = true;
    return true;
}
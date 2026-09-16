#include "physics/physics_world.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Core.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Geometry/AABox.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Body/BodyType.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "application/input.h"
#include "models/aabb.h"
#include "models/primitive.h"
#include "physics/character_capsule.h"
#include "physics/collision_shapes.h"
#include "physics/jolt_impl.h"
#include "physics/physics_utilities.h"
#include "renderer/shader/graphics_shader.h"
#include "scene/scene.h"
#include "utilities/transform.h"
#include "utilities/world_vectors.h"

namespace
{
constexpr CharacterCapsule::Settings capsuleSettings{
    .radius = 0.3f,
    .standingHeight = 1.35f,
    .crouchingHeight = 0.8f,
    .innerShapeFraction = 0.8f,
    .speed = 3.25f,
    .jumpSpeed = 3.0f,
    .sprintSpeedMultiplier = 2.1f,
    .crouchSpeedMultiplier = 0.3f,
};

bool initializePhysicsSystem(JPH::PhysicsSystem &physicsSystem, BPLayerInterfaceImpl &broadPhase,
                             ObjectVsBroadPhaseLayerFilterImpl &bfFilter, ObjectLayerPairFilterImpl &pairFilter,
                             JPH::TempAllocatorImpl &tempAllocator)
{
    // Physics engine constants
    constexpr JPH::uint maxBodies{65536u};
    constexpr JPH::uint maxBodyPairs{65536u};
    constexpr JPH::uint maxContactConstraints{10240u};

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

void PhysicsWorld::initializeScene(const Scene &scene)
{
    using namespace JPH;

    const glm::vec3 &startingPosition{scene.getCameraDescription().position};

    physics::StaticBodies collisionMeshes{physics::buildCollisionGeometry(scene, physicsSystem_.GetBodyInterface())};
    resetHeight_ = collisionMeshes.minimumHeight;
    resetPosition_ = toJPHVec3(startingPosition);
    player_.setCenter(toJPHVec3(startingPosition));
    bodyIDs_ = std::move(collisionMeshes.ids);
    physicsSystem_.OptimizeBroadPhase();

    for (const Scene::Entry &entry : scene.getAllSceneEntries())
    {
        for (const Primitive &primitive : entry.primitives)
        {
            if (primitive.shaderType != ShaderType::water && primitive.shaderType != ShaderType::ocean)
                continue;

            // Water AABBs
            for (const Transform &transform : entry.worldTransforms)
            {
                AABB translated{primitive.aabb.transform(transform.getModelMatrix())};

                // Assume 2D water plane. Create AABB with the top of the box being the water surface
                glm::vec3 halfExtent{translated.getHalfExtent()};
                halfExtent.y = std::max(halfExtent.x, halfExtent.z);

                glm::vec3 center{translated.getCenter()};
                center.y -= halfExtent.y;

                waterAABBs_.emplace_back(toJPHVec3(center - halfExtent), toJPHVec3(center + halfExtent));
            }
        }
    }

    const BodyInterface &interface{physicsSystem_.GetBodyInterface()};

    for (const BodyID &id : bodyIDs_)
    {
        uint64 userData{interface.GetUserData(id)};

        if (userData == playerID_)
            continue;

        if (interface.GetMotionType(id) != EMotionType::Dynamic)
            continue;

        // Seed starting position / rotation for starting dynamics
        MovedBody body;
        interface.GetPositionAndRotation(id, body.currentPos, body.currentRot);
        body.prevPos = body.currentPos;
        body.prevRot = body.currentRot;
        movedEntities_.emplace(id, std::move(body));

        // Tag all sphere shapes for applying drag later
        const physics::BodyIdentifier bodyIdentifier{physics::getBodyIdentifier(userData)};
        const Scene::Entry &entry{scene.viewEntry(bodyIdentifier.handle)};

        if (entry.collisionGeometry && std::holds_alternative<collision::Sphere>(entry.collisionGeometry.value()))
            spheres_.emplace(id);
    }
}

void PhysicsWorld::update(const UserInput &input, const glm::vec3 &front, float deltaTime)
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

    applyWaterBuoyancy(deltaTime);
    applySphereDrag(deltaTime);
    movePlayer(direction, front, jump, deltaTime);
    physicsSystem_.Update(deltaTime, 1, &tempAllocator_, &jobSystem_);
}

void PhysicsWorld::movePlayer(std::uint8_t direction, const glm::vec3 &front, bool jump, float deltaTime)
{
    using enum UserInput::Direction;
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

bool PhysicsWorld::footstepEvent() const
{
    if (footstepAccumulator_ >= footstepDistance_)
        return true;

    return false;
}

bool PhysicsWorld::jumpEvent() const
{
    return jumpTriggered_;
}

bool PhysicsWorld::landingEvent() const
{
    return landingTriggered_;
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

        auto iter = movedEntities_.find(id);

        if (iter == movedEntities_.end())
            continue;

        iter->second.update(interface.GetPosition(id), interface.GetRotation(id));
    }
}

void PhysicsWorld::postPhysicsSync(Scene &scene)
{
    using namespace JPH;

    const BodyInterface &interface{physicsSystem_.GetBodyInterface()};

    for (auto &[id, movedBody] : movedEntities_)
    {
        physics::BodyIdentifier sceneBody{physics::getBodyIdentifier(interface.GetUserData(id))};
        Scene::Entry &entry{scene.editEntry(sceneBody.handle)};
        Transform &transform{entry.worldTransforms[sceneBody.transformIndex]};

        transform.setAbsoluteRotation(
            glm::slerp(toGLMQuat(movedBody.prevRot), toGLMQuat(movedBody.currentRot), interpolationAlpha_));
        transform.setAbsoluteTranslation(
            glm::mix(toGLMVec3(movedBody.prevPos), toGLMVec3(movedBody.currentPos), interpolationAlpha_));
    }
}

void PhysicsWorld::applyWaterBuoyancy(float deltaTime)
{
    using namespace JPH;

    constexpr float linearDrag{0.3f};
    constexpr float angularDrag{0.1f};

    if (waterAABBs_.empty())
        return;

    AllHitCollisionCollector<CollideShapeBodyCollector> collector;

    for (const AABox &box : waterAABBs_)
    {
        physicsSystem_.GetBroadPhaseQuery().CollideAABox(box, collector,
                                                         SpecifiedBroadPhaseLayerFilter(BroadPhaseLayers::MOVING),
                                                         SpecifiedObjectLayerFilter(Layers::MOVING));

        RVec3 center{box.GetCenter()};
        const RVec3 extent{box.GetExtent()};
        center.SetY(center.GetY() + extent.GetY());

        for (const BodyID &id : collector.mHits)
        {
            BodyLockWrite lock{physicsSystem_.GetBodyLockInterface(), id};

            if (!lock.Succeeded())
                continue;

            Body &body{lock.GetBody()};
            const Shape *shape{body.GetShape()};

            body.ApplyBuoyancyImpulse(center, Vec3{0, 1, 0}, physics::getDensity(shape->GetUserData()), linearDrag,
                                      angularDrag, Vec3::sZero(), physicsSystem_.GetGravity(), deltaTime);
        }

        collector.Reset();
    }
}

void PhysicsWorld::applySphereDrag(float deltaTime)
{
    using namespace JPH;

    constexpr float linearVelocityMultiplier{0.99f};

    // Speed in m/s before applying extra stopping power
    constexpr float stopThreshold{1.0f};

    // Deceleration  in m/s
    constexpr float stopRate{0.3f};

    BodyInterface &interface{physicsSystem_.GetBodyInterface()};

    for (const BodyID &id : spheres_)
    {
        BodyLockWrite lock{physicsSystem_.GetBodyLockInterface(), id};

        if (!lock.Succeeded())
            continue;

        Body &body{lock.GetBody()};

        if (!body.IsActive() || !body.IsDynamic())
            continue;

        const float radius{body.GetShape()->GetInnerRadius()};
        const RRayCast ray{body.GetCenterOfMassPosition(), Vec3{0.0, -(radius + 0.05f), 0.0f}};

        RayCastResult hit;
        const bool touchingGround{physicsSystem_.GetNarrowPhaseQueryNoLock().CastRay(
            ray, hit, SpecifiedBroadPhaseLayerFilter{BroadPhaseLayers::NON_MOVING},
            SpecifiedObjectLayerFilter{Layers::NON_MOVING}, IgnoreSingleBodyFilter{id})};

        if (!touchingGround)
            continue;

        Vec3 velocity{body.GetLinearVelocity()};
        const float speed{velocity.Length()};
        float scale{linearVelocityMultiplier};

        if (speed < stopThreshold)
        {
            const float newSpeed{speed - stopRate * deltaTime};
            scale = newSpeed > 0.0f ? newSpeed / speed : 0.0f;
        }

        body.SetLinearVelocity(velocity * scale);
        body.SetAngularVelocity(body.GetAngularVelocity() * scale);
    }
}

const std::vector<physics::ObjectCollision> &PhysicsWorld::getCollisions() const
{
    return player_.getContactedBodies();
}

void PhysicsWorld::clearFlags()
{
    jumpTriggered_ = false;
    landingTriggered_ = false;

    if (footstepAccumulator_ >= footstepDistance_)
        footstepAccumulator_ = std::fmod(footstepAccumulator_, footstepDistance_);
}
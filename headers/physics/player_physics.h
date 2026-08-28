#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

#include "application/input.h"
#include "physics/character_capsule.h"
#include "scene/scene.h"

class PlayerPhysics
{
  public:
    PlayerPhysics(JPH::PhysicsSystem &physicsSystem, JPH::TempAllocatorImpl &tempAllocator);
    PlayerPhysics(const PlayerPhysics &) = delete;
    PlayerPhysics &operator=(const PlayerPhysics &) = delete;
    PlayerPhysics(PlayerPhysics &&other) = delete;
    PlayerPhysics &operator=(PlayerPhysics &&other) = delete;
    ~PlayerPhysics();

    void initializeScene(Scene &scene);

    bool isGrounded() const;
    bool isSupported() const;
    glm::vec3 getVelocity() const;
    glm::vec3 getPosition() const;
    glm::vec3 getInterpolatedPosition() const;
    void setPosition(const glm::vec3 &newPosition);

    void update(const Input &input, const glm::vec3 &front, float deltaTime);
    bool footstepEvent();
    bool jumpEvent();
    bool landingEvent();

  private:
    static constexpr float physicsTick_{1.0f / 60.0f};
    static constexpr float footstepDistance_{2.0f};
    static constexpr float distancePrimer_{0.1f};
    static constexpr float maxAccumulator_{physicsTick_ * 3.0f};

    bool jumpTriggered_{false};
    bool landingTriggered_{false};
    float jumpAccumulator_{0.0f};
    float timeAccumulator_{0.0f};
    float footstepAccumulator_{footstepDistance_ -
                               distancePrimer_}; // Prime accumulator so that a footstep fires quickly
    float resetHeight_{0.0f};
    JPH::Vec3 previousPosition_{0.0f, 0.0f, 0.0f};
    JPH::Vec3 resetPosition_{0.0f, 0.0f, 0.0f};

    std::vector<JPH::BodyID> bodyIDs_;
    JPH::PhysicsSystem &physicsSystem_;
    JPH::BodyInterface *bodyInterface_;
    JPH::TempAllocatorImpl &tempAllocator_;

    // The player capsule
    CharacterCapsule player_;

    void movePlayer(std::uint8_t direction, const glm::vec3 &front, bool jump, float deltaTime);
    void tick(std::uint8_t direction, const glm::vec3 &front, bool jump, float deltaTime);
};
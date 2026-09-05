#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/Core.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <cstdint>
#include <glm/glm.hpp>
#include <limits>
#include <unordered_map>
#include <vector>

#include "application/input.h"
#include "physics/character_capsule.h"
#include "physics/jolt_impl.h"
#include "scene/scene.h"

class PhysicsWorld
{
  public:
    PhysicsWorld();
    PhysicsWorld(const PhysicsWorld &) = delete;
    PhysicsWorld &operator=(const PhysicsWorld &) = delete;
    PhysicsWorld(PhysicsWorld &&other) = delete;
    PhysicsWorld &operator=(PhysicsWorld &&other) = delete;
    ~PhysicsWorld();

    void initializeScene(const Scene &scene);

    bool playerIsGrounded() const;
    bool playerIsSupported() const;
    glm::vec3 getPlayerVelocity() const;
    glm::vec3 getPlayerPosition() const;
    void setPlayerPosition(const glm::vec3 &newPosition);

    void update(const Input &input, const glm::vec3 &front, float deltaTime);
    bool footstepEvent();
    bool jumpEvent();
    bool landingEvent();

    void synchronizeEntries(Scene &scene);

  private:
    struct MovedBody
    {
        JPH::Vec3 prevPos{JPH::Vec3::sZero()};
        JPH::Vec3 currentPos{JPH::Vec3::sZero()};

        JPH::Quat prevRot{JPH::Quat::sIdentity()};
        JPH::Quat currentRot{JPH::Quat::sIdentity()};

        void update(const JPH::Vec3 &pos, const JPH::Quat &rot)
        {
            prevPos = currentPos;
            prevRot = currentRot;
            currentPos = pos;
            currentRot = rot;
        }
    };

    static constexpr size_t joltStackSize_{10 * 1024 * 1024}; // 10 MB
    static constexpr JPH::uint maxPhysicsJobs_{2048u};

    static constexpr float physicsTick_{1.0f / 60.0f};
    static constexpr float footstepDistance_{2.0f};
    static constexpr float distancePrimer_{0.1f};
    static constexpr float maxAccumulator_{physicsTick_ * 3.0f};
    static constexpr JPH::uint64 playerID_{std::numeric_limits<JPH::uint64>::max()};

    float interpolationAlpha_{0.0f};

    bool jumpTriggered_{false};
    bool landingTriggered_{false};
    float jumpAccumulator_{0.0f};
    float timeAccumulator_{0.0f};
    float footstepAccumulator_{footstepDistance_ -
                               distancePrimer_}; // Prime accumulator so that a footstep fires quickly
    float resetHeight_{0.0f};
    JPH::Vec3 previousPosition_{0.0f, 0.0f, 0.0f};
    JPH::Vec3 resetPosition_{0.0f, 0.0f, 0.0f};

    // Jolt setup
    BPLayerInterfaceImpl broadPhaseLayer_;
    ObjectLayerPairFilterImpl objectLayerPairFilter_;
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter_;
    JPH::TempAllocatorImpl tempAllocator_{joltStackSize_};
    JPH::JobSystemThreadPool jobSystem_{maxPhysicsJobs_, JPH::cMaxPhysicsBarriers};

    // Critical member, must be destroyed before the layers and filters
    JPH::PhysicsSystem physicsSystem_;

    // Annoying workaround to call Jolt's Init from the constructor
    bool initialized_{false};

    // The player capsule
    CharacterCapsule player_;

    // Dirty list of moved entities
    std::unordered_map<JPH::BodyID, MovedBody> movedEntities_;
    std::vector<JPH::BodyID> bodyIDs_;
    JPH::BodyIDVector activeBodies_;
    std::unordered_set<JPH::BodyID> spheres_;

    // Water bodies
    std::vector<JPH::AABox> waterAABBs_;

    void movePlayer(std::uint8_t direction, const glm::vec3 &front, bool jump, float deltaTime);
    void tick(std::uint8_t direction, const glm::vec3 &front, bool jump, float deltaTime);
    void flagMovedEntities();
    void applyWaterBuoyancy(float deltaTime);
    void applySphereDrag(float deltaTime);
};
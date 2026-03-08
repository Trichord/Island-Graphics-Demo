#pragma once

#include <cstdint>

#include "jolt/Jolt.h"

#include "glm/glm.hpp"
#include "jolt/Core/Core.h"
#include "jolt/Core/JobSystemSingleThreaded.h"
#include "jolt/Core/TempAllocator.h"
#include "jolt/Geometry/Triangle.h"
#include "jolt/Math/Vec3.h"
#include "jolt/Physics/Body/Body.h"
#include "jolt/Physics/Body/BodyInterface.h"
#include "jolt/Physics/PhysicsSystem.h"
#include "jolt_impl.h"
#include "player.h"
#include "scene.h"

enum CharacterMovement : std::uint8_t
{
    FORWARD = 1U,
    BACKWARD = 2U,
    LEFT = 4U,
    RIGHT = 8U,
    NONE = 16U
};

constexpr float physicsTick{1.0f / 60.0f};

class PlayerPhysics
{
  public:
    PlayerPhysics(Scene &scene, JPH::PhysicsSystem &physicsSystem, JPH::TempAllocatorImpl &tempAllocator);
    PlayerPhysics(const PlayerPhysics &) = delete;
    PlayerPhysics &operator=(const PlayerPhysics &) = delete;
    PlayerPhysics(PlayerPhysics &&other) = delete;
    PlayerPhysics &operator=(PlayerPhysics &&other) = delete;
    ~PlayerPhysics();

    glm::vec3 getPosition() const;
    const glm::vec3 &getPreviousPosition() const;
    void setPosition(const glm::vec3 &newPosition);

    void toggleCrouch(bool enable);
    void toggleSprint(bool enable);

    void update(std::uint8_t direction, const glm::vec3 &front, bool jump, bool crouch, float deltaTime);

  private:
    static constexpr JPH::uint maxPhysicsJobs_{2048};

    float resetHeight_{0.0f};
    glm::vec3 previousPosition_{0.0f};
    JPH::Vec3 resetPosition_{0.0f, 0.0f, 0.0f};

    JPH::PhysicsSystem &physicsSystem_;

    // Everything the physics system needs
    JPH::BodyInterface *bodyInterface_;
    BPLayerInterfaceImpl broadPhaseLayer_;
    JPH::TempAllocatorImpl &tempAllocator_;
    ObjectLayerPairFilterImpl objectLayerPairFilter_;
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter_;
    JPH::JobSystemSingleThreaded jobSystem_{maxPhysicsJobs_};
    
    // Physics body for terrain
    JPH::Body *terrain_{nullptr};

    //The player capsule
    Player player_;

    JPH::TriangleList traverseSceneGraph(Scene &scene);
    void movePlayer(std::uint8_t direction, const glm::vec3 &front, bool jump, bool crouch, float deltaTime);
};
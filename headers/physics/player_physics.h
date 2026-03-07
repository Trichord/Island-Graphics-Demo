#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "jolt/Jolt.h"

#include "glm/glm.hpp"
#include "jolt/Core/JobSystemSingleThreaded.h"
#include "jolt/Core/TempAllocator.h"
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

    void update(std::uint8_t direction, const glm::vec3 &front, float deltaTime);

    glm::vec3 getPosition() const;
    const glm::vec3 &getPreviousPosition() const;
    void setPosition(const glm::vec3 &newPosition);

  private:
    static constexpr JPH::uint maxPhysicsJobs_{2048};

    glm::vec3 previousPosition_{0.0f};
    JPH::Vec3 resetPosition_{0.0f, 0.0f, 0.0f};
    float resetHeight_{0.0f};

    JPH::PhysicsSystem &physicsSystem_;

    // Everything the physics system needs
    JPH::TempAllocatorImpl &tempAllocator_;
    ObjectLayerPairFilterImpl objectLayerPairFilter_;
    BPLayerInterfaceImpl broadPhaseLayer_;
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter_;
    JPH::JobSystemSingleThreaded jobSystem_{maxPhysicsJobs_};

    JPH::BodyInterface *bodyInterface_;

    // Physics body for terrain
    JPH::Body *terrain_{nullptr};

    Player player_;

    void movePlayer(std::uint8_t direction, const glm::vec3 &front, float deltaTime);
    JPH::TriangleList traverseSceneGraph(Scene &scene);
};
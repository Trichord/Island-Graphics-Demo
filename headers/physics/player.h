#pragma once

#include "jolt/Jolt.h"

#include "glm/glm.hpp"
#include "jolt/Core/Reference.h"
#include "jolt/Physics/Character/Character.h"
#include "jolt/Physics/Character/CharacterVirtual.h"
#include "jolt/Physics/Collision/Shape/Shape.h"

class Player
{
  public:
    Player() = default;
    Player(const Player &) = delete;
    Player &operator=(const Player &) = delete;
    Player(Player &&other) = delete;
    Player &operator=(Player &&other) = delete;

    void init(JPH::PhysicsSystem &physicsSystem, JPH::TempAllocatorImpl &tempAllocator);
    JPH::Vec3 getCenter() const;
    JPH::Vec3 getPosition() const;
    void setPosition(JPH::Vec3 position);
    void handleInput(const JPH::Vec3 &direction, bool jump, bool switchStance, float deltaTime);
    void prePhysicsUpdate(float deltaTime);

  private:
    // Character size
    static constexpr float characterHeightStanding_{1.35f};
    static constexpr float characterRadiusStanding_{0.5f};
    static constexpr float characterHeightCrouching_{0.8f};
    static constexpr float characterRadiusCrouching_{0.5f};
    static constexpr float innerShapeFraction_{0.9f};

    // Terrain interaction
    static constexpr float maxSlopeAngle_{glm::radians(45.0f)};

    // Character movement properties
    bool controlMovementDuringJump_{true}; // If false the character cannot change movement direction in mid air
    bool allowSliding_{false};
    bool enableCharacterInertia_{true};
    float characterSpeed_{6.0f};
    float jumpSpeed_{4.0f};
    JPH::Vec3 desiredVelocity_{0.0f, 0.0f, 0.0f};

    // The different stances for the character
    JPH::RefConst<JPH::Shape> standingShape_;
    JPH::RefConst<JPH::Shape> crouchingShape_;
    JPH::RefConst<JPH::Shape> innerCrouchingShape_;
    JPH::RefConst<JPH::Shape> innerStandingShape_;

    JPH::Ref<JPH::CharacterVirtual> player_;

    // Non-owning pointers. PlayerPhysics is the only one who should ever have a Player class, and both should be freed
    // together
    JPH::PhysicsSystem *physicsSystem_{nullptr};
    JPH::TempAllocatorImpl *tempAllocator_{nullptr};
};
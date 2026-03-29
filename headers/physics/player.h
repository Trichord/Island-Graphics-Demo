#pragma once

#include "jolt/Jolt.h"

#include "glm/glm.hpp"
#include "jolt/Core/Reference.h"
#include "jolt/Core/TempAllocator.h"
#include "jolt/Math/Vec3.h"
#include "jolt/Physics/Character/Character.h"
#include "jolt/Physics/Character/CharacterVirtual.h"
#include "jolt/Physics/Collision/Shape/Shape.h"
#include "jolt/Physics/PhysicsSystem.h"

class Player : public JPH::CharacterContactListener
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
    void setVelocity(JPH::Vec3 velocity);
    void toggleCrouch(bool enable);
    void toggleSprint(bool enable);

    void physicsUpdate(float deltaTime);
    void handleInput(const JPH::Vec3 &direction, bool jump, float deltaTime);

    void OnContactSolve(const JPH::CharacterVirtual *inCharacter, const JPH::BodyID &inBodyID2,
                        const JPH::SubShapeID &inSubShapeID2, JPH::RVec3Arg inContactPosition,
                        JPH::Vec3Arg inContactNormal, JPH::Vec3Arg inContactVelocity,
                        const JPH::PhysicsMaterial *inContactMaterial, JPH::Vec3Arg inCharacterVelocity,
                        JPH::Vec3 &ioNewCharacterVelocity) override;

  private:
    // Character size
    static constexpr float characterHeightStanding_{1.35f};
    static constexpr float characterRadiusStanding_{0.3f};
    static constexpr float characterHeightCrouching_{0.8f};
    static constexpr float characterRadiusCrouching_{0.3f};
    static constexpr float innerShapeFraction_{0.9f};

    // Terrain interaction
    static constexpr float maxSlopeAngle_{JPH::DegreesToRadians(45.0f)};

    // Character movement properties
    static constexpr float jumpSpeed_{2.75f};
    static constexpr float characterSpeed_{4.0f};
    static constexpr float characterCrouchSpeedMultiplier{0.4f};
    static constexpr float characterSprintSpeedMultiplier_{1.4f};
    bool crouched_{false};
    bool allowSliding_{false};
    bool characterSprinting_{false};
    bool enableCharacterInertia_{true};
    bool controlMovementDuringJump_{true}; // If false the character cannot change movement direction in mid air

    JPH::Vec3 desiredVelocity_{0.0f, 0.0f, 0.0f};

    // The different stances for the character
    JPH::RefConst<JPH::Shape> standingShape_;
    JPH::RefConst<JPH::Shape> innerStandingShape_;
    JPH::RefConst<JPH::Shape> crouchingShape_;
    JPH::RefConst<JPH::Shape> innerCrouchingShape_;

    // Capsule
    JPH::Ref<JPH::CharacterVirtual> player_;

    // Non-owning pointers
    JPH::PhysicsSystem *physicsSystem_{nullptr};
    JPH::TempAllocatorImpl *tempAllocator_{nullptr};
};
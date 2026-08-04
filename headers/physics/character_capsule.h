#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/Reference.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Math.h>
#include <Jolt/Math/MathTypes.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <glm/glm.hpp>

class CharacterCapsule : public JPH::CharacterContactListener
{
  public:
    CharacterCapsule() = default;
    CharacterCapsule(JPH::PhysicsSystem &physicsSystem, JPH::TempAllocatorImpl &tempAllocator);
    CharacterCapsule(const CharacterCapsule &) = delete;
    CharacterCapsule &operator=(const CharacterCapsule &) = delete;
    CharacterCapsule(CharacterCapsule &&other) = delete;
    CharacterCapsule &operator=(CharacterCapsule &&other) = delete;

    float getRadius() const;
    JPH::Vec3 getCenter() const;
    JPH::Vec3 getPosition() const;
    JPH::Vec3 getVelocity() const;
    bool isGrounded() const;

    void setCenter(const JPH::Vec3 &center);
    void setPosition(const JPH::Vec3& position);
    void setVelocity(const JPH::Vec3 &velocity);
    void toggleCrouch(bool enable);
    void toggleSprint(bool enable);

    void physicsUpdate(float deltaTime);
    void handleInput(const JPH::Vec3 &direction, bool jump, float deltaTime);

    void OnContactSolve(const JPH::CharacterVirtual *inCharacter, const JPH::BodyID &inBodyID2,
                        const JPH::SubShapeID &inSubShapeID2, JPH::RVec3Arg inContactPosition,
                        JPH::Vec3Arg inContactNormal, JPH::Vec3Arg inContactVelocity,
                        const JPH::PhysicsMaterial *inContactMaterial, JPH::Vec3Arg inCharacterVelocity,
                        JPH::Vec3 &ioNewCharacterVelocity) override;

    bool OnContactValidate(const JPH::CharacterVirtual *inCharacter, const JPH::CharacterContact &inContact) override;

  private:
    // Character size
    static constexpr float characterRadius_{0.3f};
    static constexpr float characterHeightStanding_{1.35f};
    static constexpr float characterHeightCrouching_{0.8f};
    static constexpr float innerShapeFraction_{0.9f};

    // Terrain interaction
    static constexpr float maxSlopeAngle_{JPH::DegreesToRadians(50.0f)};

    // Character movement properties
    static constexpr float jumpSpeed_{2.75f};
    static constexpr float characterSpeed_{3.5f};
    static constexpr float characterCrouchSpeedMultiplier_{0.4f};
    static constexpr float characterSprintSpeedMultiplier_{1.45f};

    bool crouched_{false};
    bool allowSliding_{false};
    bool characterSprinting_{false};
    bool controlMovementDuringJump_{true};

    JPH::Vec3 desiredVelocity_{0.0f, 0.0f, 0.0f};

    // Non-owning pointers
    JPH::PhysicsSystem *physicsSystem_{nullptr};
    JPH::TempAllocatorImpl *tempAllocator_{nullptr};

    // The different stances for the character
    JPH::RefConst<JPH::Shape> standingShape_;
    JPH::RefConst<JPH::Shape> innerStandingShape_;
    JPH::RefConst<JPH::Shape> crouchingShape_;
    JPH::RefConst<JPH::Shape> innerCrouchingShape_;

    // Capsule
    JPH::Ref<JPH::CharacterVirtual> capsule_;
};
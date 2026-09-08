#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/Core.h>
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
#include <vector>
#include <glm/glm.hpp>
#include <optional>

#include "physics/physics_utilities.h"

class CharacterCapsule : public JPH::CharacterContactListener
{
  public:
    struct Settings
    {
        float radius{0.3f};
        float standingHeight{1.0f};
        float crouchingHeight{0.8f};
        float innerShapeFraction{0.9f};

        float speed{1.0f};
        float jumpSpeed{1.0f};
        float sprintSpeedMultiplier{1.5f};
        float crouchSpeedMultiplier{0.75f};
    };

    CharacterCapsule() = default;
    CharacterCapsule(JPH::PhysicsSystem &physicsSystem, JPH::TempAllocatorImpl &tempAllocator, Settings capsuleSettings,
                     JPH::uint64 userID);
    CharacterCapsule(const CharacterCapsule &) = delete;
    CharacterCapsule &operator=(const CharacterCapsule &) = delete;
    CharacterCapsule(CharacterCapsule &&other) = delete;
    CharacterCapsule &operator=(CharacterCapsule &&other) = delete;

    float getRadius() const;
    JPH::Vec3 getCenter() const;
    JPH::Vec3 getPosition() const;
    JPH::Vec3 getVelocity() const;
    JPH::BodyID getBodyID() const;
    bool isSupported() const;
    bool isGrounded() const;
    const std::vector<physics::ObjectCollision> &getContactedBodies() const;

    void setCenter(const JPH::Vec3 &center);
    void setPosition(const JPH::Vec3 &position);
    void setVelocity(const JPH::Vec3 &velocity);
    void toggleCrouch(bool enable);
    void toggleSprint(bool enable);

    void physicsUpdate(float deltaTime);
    void handleInput(const JPH::Vec3 &direction, bool jump, float deltaTime);
    std::optional<float> predictImpactSpeed(float lookAheadTime) const;

  private:
    // Terrain interaction
    static constexpr float maxSlopeAngle_{JPH::DegreesToRadians(50.0f)};

    bool crouched_{false};
    bool allowSliding_{false};
    bool characterSprinting_{false};
    bool controlMovementDuringJump_{true};

    JPH::Vec3 desiredVelocity_{0.0f, 0.0f, 0.0f};
    Settings capsuleSettings_;

    // Non-owning pointers
    JPH::PhysicsSystem &physicsSystem_;
    JPH::TempAllocatorImpl &tempAllocator_;

    // The different stances for the character
    JPH::RefConst<JPH::Shape> standingShape_;
    JPH::RefConst<JPH::Shape> innerStandingShape_;
    JPH::RefConst<JPH::Shape> crouchingShape_;
    JPH::RefConst<JPH::Shape> innerCrouchingShape_;

    // Capsule
    JPH::Ref<JPH::CharacterVirtual> capsule_;

    // Per frame list of collided with objects. Using a vector because the number of contacts should be low per frame
    std::vector<physics::ObjectCollision> touchedObjects_;

    // Jolt callbacks
    void OnContactSolve(const JPH::CharacterVirtual *inCharacter, const JPH::BodyID &inBodyID2,
                        const JPH::SubShapeID &inSubShapeID2, JPH::RVec3Arg inContactPosition,
                        JPH::Vec3Arg inContactNormal, JPH::Vec3Arg inContactVelocity,
                        const JPH::PhysicsMaterial *inContactMaterial, JPH::Vec3Arg inCharacterVelocity,
                        JPH::Vec3 &ioNewCharacterVelocity) override;
    bool OnContactValidate(const JPH::CharacterVirtual *inCharacter, const JPH::CharacterContact &inContact) override;
};
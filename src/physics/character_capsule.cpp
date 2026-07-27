#include "physics/character_capsule.h"

#include <jolt/Core/Reference.h>
#include <jolt/Core/TempAllocator.h>
#include <jolt/Geometry/Plane.h>
#include <jolt/Math/MathTypes.h>
#include <jolt/Math/Quat.h>
#include <jolt/Math/Real.h>
#include <jolt/Math/Vec3.h>
#include <jolt/Physics/Body/BodyID.h>
#include <jolt/Physics/Character/CharacterBase.h>
#include <jolt/Physics/Character/CharacterVirtual.h>
#include <jolt/Physics/Collision/PhysicsMaterial.h>
#include <jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <jolt/Physics/Collision/Shape/Shape.h>
#include <jolt/Physics/Collision/Shape/SubShapeID.h>
#include <jolt/Physics/PhysicsSystem.h>

#include "physics/jolt_impl.h"

namespace
{
JPH::ShapeRefC createShape(float height, float radius)
{
    using namespace JPH;

    return RotatedTranslatedShapeSettings(Vec3(0, 0.5f * height + radius, 0), Quat::sIdentity(),
                                          new CapsuleShape(0.5f * height, radius))
        .Create()
        .Get();
}
} // namespace

CharacterCapsule::CharacterCapsule(JPH::PhysicsSystem &physicsSystem, JPH::TempAllocatorImpl &tempAllocator)
    : physicsSystem_{&physicsSystem}, tempAllocator_{&tempAllocator},
      standingShape_{createShape(characterHeightStanding_, characterRadiusStanding_)},
      crouchingShape_{createShape(characterHeightCrouching_, characterRadiusCrouching_)},
      innerStandingShape_{
          createShape(innerShapeFraction_ * characterHeightStanding_, innerShapeFraction_ * characterRadiusStanding_)},
      innerCrouchingShape_{
          createShape(innerShapeFraction_ * characterHeightCrouching_, innerShapeFraction_ * characterRadiusCrouching_)}
{
    using namespace JPH;

    Ref<CharacterVirtualSettings> settings = new CharacterVirtualSettings();
    settings->mMaxSlopeAngle = maxSlopeAngle_;
    settings->mShape = standingShape_;
    settings->mSupportingVolume =
        Plane(Vec3::sAxisY(), -characterRadiusStanding_); // Accept contacts that touch the lower sphere of the capsule
    settings->mEnhancedInternalEdgeRemoval = true;
    settings->mInnerBodyShape = innerStandingShape_;
    settings->mInnerBodyLayer = Layers::MOVING;

    // Create the player's capsule
    capsule_ = new CharacterVirtual(settings, RVec3::sZero(), Quat::sIdentity(), 0, &physicsSystem);
    capsule_->SetListener(this);
}

// Returns the center of mass of the capsule
JPH::Vec3 CharacterCapsule::getCenter() const
{
    return capsule_->GetCenterOfMassPosition();
}

// Returns the bottom of the capsule, per JPH
JPH::Vec3 CharacterCapsule::getPosition() const
{
    return capsule_->GetPosition();
}

void CharacterCapsule::setPosition(JPH::Vec3 position)
{
    capsule_->SetPosition(position);
}

void CharacterCapsule::setVelocity(JPH::Vec3 velocity)
{
    capsule_->SetLinearVelocity(velocity);
    desiredVelocity_ = JPH::Vec3::sZero();
}

void CharacterCapsule::toggleCrouch(bool enable)
{
    using namespace JPH;

    if (crouched_ == enable)
        return;

    crouched_ = !crouched_;

    // Toggle crouch based off the current shape of the capsule
    bool isStanding{capsule_->GetShape() == standingShape_};
    const Shape *shape{isStanding ? crouchingShape_ : standingShape_};

    if (capsule_->SetShape(shape, 1.5f * physicsSystem_->GetPhysicsSettings().mPenetrationSlop,
                           physicsSystem_->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
                           physicsSystem_->GetDefaultLayerFilter(Layers::MOVING), {}, {}, *tempAllocator_))
    {
        const Shape *inner_shape{isStanding ? innerCrouchingShape_ : innerStandingShape_};
        capsule_->SetInnerBodyShape(inner_shape);
    }
}

void CharacterCapsule::toggleSprint(bool enable)
{
    if (characterSprinting_ == enable)
        return;

    if (capsule_->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround)
        characterSprinting_ = !characterSprinting_;
}

void CharacterCapsule::handleInput(const JPH::Vec3 &direction, bool jump, float deltaTime)
{
    using namespace JPH;

    const bool playerControlsHorizontalVelocity{controlMovementDuringJump_ || capsule_->IsSupported()};

    if (playerControlsHorizontalVelocity)
    {
        float speed{0.0f};

        if (characterSprinting_ && !crouched_)
            speed = characterSpeed_ * characterSprintSpeedMultiplier_;
        else
            speed = characterSpeed_;

        if (crouched_ && capsule_->IsSupported())
            speed *= characterCrouchSpeedMultiplier_;

        // Smooth the player input
        desiredVelocity_ = 0.3f * direction * speed + 0.7f * desiredVelocity_;

        // True if the player intended to move
        allowSliding_ = !direction.IsNearZero();
    }
    else
    {
        // While in air we allow sliding
        allowSliding_ = true;
    }

    // Current velocity
    Vec3 currentVerticalVelocity{capsule_->GetLinearVelocity().Dot(capsule_->GetUp()) * capsule_->GetUp()};
    Vec3 groundVelocity{capsule_->GetGroundVelocity()};
    bool movingTowardsGround{(currentVerticalVelocity.GetY() - groundVelocity.GetY()) < 0.1f};

    Vec3 newVelocity;

    if (capsule_->GetGroundState() == CharacterVirtual::EGroundState::OnGround // If on ground
        && (enableCharacterInertia_
                ? movingTowardsGround // Inertia enabled: And not moving away from ground
                : !capsule_->IsSlopeTooSteep(
                      capsule_->GetGroundNormal()))) // Inertia disabled: And not on a slope that is too steep
    {
        // Assume velocity of ground when on ground
        newVelocity = groundVelocity;

        // Jump
        if (jump && movingTowardsGround)
            newVelocity += jumpSpeed_ * capsule_->GetUp();
    }
    else
        newVelocity = currentVerticalVelocity;

    // Gravity
    newVelocity += physicsSystem_->GetGravity() * deltaTime;

    if (playerControlsHorizontalVelocity)
    {
        // Player input
        newVelocity += desiredVelocity_;
    }
    else
    {
        // Preserve horizontal velocity
        Vec3 currentHorizontalVelocity = capsule_->GetLinearVelocity() - currentVerticalVelocity;
        newVelocity += currentHorizontalVelocity;
    }

    newVelocity = capsule_->CancelVelocityTowardsSteepSlopes(newVelocity);

    // Update character velocity
    capsule_->SetLinearVelocity(newVelocity);
}

void CharacterCapsule::physicsUpdate(float deltaTime)
{
    using namespace JPH;

    // Move character, using default update settings
    CharacterVirtual::ExtendedUpdateSettings updateSettings;
    capsule_->ExtendedUpdate(deltaTime, physicsSystem_->GetGravity(), updateSettings,
                             physicsSystem_->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
                             physicsSystem_->GetDefaultLayerFilter(Layers::MOVING), {}, {}, *tempAllocator_);
}

void CharacterCapsule::OnContactSolve(const JPH::CharacterVirtual *inCharacter, const JPH::BodyID &inBodyID2,
                                      const JPH::SubShapeID &inSubShapeID2, JPH::RVec3Arg inContactPosition,
                                      JPH::Vec3Arg inContactNormal, JPH::Vec3Arg inContactVelocity,
                                      const JPH::PhysicsMaterial *inContactMaterial, JPH::Vec3Arg inCharacterVelocity,
                                      JPH::Vec3 &ioNewCharacterVelocity)
{
    // Ignore callbacks for other characters than the player
    if (inCharacter != capsule_)
        return;

    // Cancel out any vertical component to prevent steep slope jitter
    if (inCharacter->IsSlopeTooSteep(inContactNormal))
    {
        JPH::Vec3 up{inCharacter->GetUp()};
        float vertical{ioNewCharacterVelocity.Dot(up)};
        if (vertical > 0.0f)
            ioNewCharacterVelocity -= vertical * up;
    }

    // Don't allow the player to slide down static not-too-steep surfaces when not actively moving and when not on a
    // moving platform
    if (!allowSliding_ && inContactVelocity.IsNearZero() && !inCharacter->IsSlopeTooSteep(inContactNormal))
        ioNewCharacterVelocity = JPH::Vec3::sZero();
}

JPH::Vec3 CharacterCapsule::getVelocity() const
{
    return capsule_->GetLinearVelocity();
}
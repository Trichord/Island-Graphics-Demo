#include "player.h"

#include "jolt/Core/Reference.h"
#include "jolt/Core/TempAllocator.h"
#include "jolt/Geometry/Plane.h"
#include "jolt/Math/Quat.h"
#include "jolt/Math/Real.h"
#include "jolt/Math/Vec3.h"
#include "jolt/Physics/Character/CharacterBase.h"
#include "jolt/Physics/Character/CharacterVirtual.h"
#include "jolt/Physics/Collision/Shape/CapsuleShape.h"
#include "jolt/Physics/Collision/Shape/RotatedTranslatedShape.h"
#include "jolt/Physics/Collision/Shape/Shape.h"
#include "jolt/Physics/PhysicsSystem.h"
#include "jolt_impl.h"
#include <jolt/Math/MathTypes.h>
#include <jolt/Physics/Body/BodyID.h>
#include <jolt/Physics/Collision/PhysicsMaterial.h>
#include <jolt/Physics/Collision/Shape/SubShapeID.h>

void Player::init(JPH::PhysicsSystem &physicsSystem, JPH::TempAllocatorImpl &tempAllocator)
{
    using namespace JPH;

    physicsSystem_ = &physicsSystem;
    tempAllocator_ = &tempAllocator;

    // Capsule shapes for all player stances
    standingShape_ = RotatedTranslatedShapeSettings(
                         Vec3(0, 0.5f * characterHeightStanding_ + characterRadiusStanding_, 0), Quat::sIdentity(),
                         new CapsuleShape(0.5f * characterHeightStanding_, characterRadiusStanding_))
                         .Create()
                         .Get();
    crouchingShape_ = RotatedTranslatedShapeSettings(
                          Vec3(0, 0.5f * characterHeightCrouching_ + characterRadiusCrouching_, 0), Quat::sIdentity(),
                          new CapsuleShape(0.5f * characterHeightCrouching_, characterRadiusCrouching_))
                          .Create()
                          .Get();
    innerStandingShape_ = RotatedTranslatedShapeSettings(
                              Vec3(0, 0.5f * characterHeightStanding_ + characterRadiusStanding_, 0), Quat::sIdentity(),
                              new CapsuleShape(0.5f * innerShapeFraction_ * characterHeightStanding_,
                                               innerShapeFraction_ * characterRadiusStanding_))
                              .Create()
                              .Get();
    innerCrouchingShape_ =
        RotatedTranslatedShapeSettings(Vec3(0, 0.5f * characterHeightCrouching_ + characterRadiusCrouching_, 0),
                                       Quat::sIdentity(),
                                       new CapsuleShape(0.5f * innerShapeFraction_ * characterHeightCrouching_,
                                                        innerShapeFraction_ * characterRadiusCrouching_))
            .Create()
            .Get();

    // Settings for the player capsule
    Ref<CharacterVirtualSettings> settings = new CharacterVirtualSettings();
    settings->mMaxSlopeAngle = maxSlopeAngle_;
    settings->mShape = standingShape_;
    settings->mSupportingVolume =
        Plane(Vec3::sAxisY(), -characterRadiusStanding_); // Accept contacts that touch the lower sphere of the capsule
    settings->mEnhancedInternalEdgeRemoval = true;
    settings->mInnerBodyShape = innerStandingShape_;
    settings->mInnerBodyLayer = Layers::MOVING;

    // Create the player's capsule
    player_ = new CharacterVirtual(settings, RVec3::sZero(), Quat::sIdentity(), 0, &physicsSystem);
    player_->SetListener(this);
}

// Returns the center of mass of the capsule
JPH::Vec3 Player::getCenter() const
{
    return player_->GetCenterOfMassPosition();
}

// Returns the bottom of the capsule, per JPH
JPH::Vec3 Player::getPosition() const
{
    return player_->GetPosition();
}

void Player::setPosition(JPH::Vec3 position)
{
    player_->SetPosition(position);
}

void Player::setVelocity(JPH::Vec3 velocity)
{
    player_->SetLinearVelocity(velocity);
    desiredVelocity_ = JPH::Vec3::sZero();
}

void Player::toggleCrouch(bool enable)
{
    using namespace JPH;

    if (crouched_ == enable)
        return;

    crouched_ = !crouched_;

    // Toggle crouch based off the current shape of the capsule
    bool isStanding{player_->GetShape() == standingShape_};
    const Shape *shape{isStanding ? crouchingShape_ : standingShape_};

    if (player_->SetShape(shape, 1.5f * physicsSystem_->GetPhysicsSettings().mPenetrationSlop,
                          physicsSystem_->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
                          physicsSystem_->GetDefaultLayerFilter(Layers::MOVING), {}, {}, *tempAllocator_))
    {
        const Shape *inner_shape{isStanding ? innerCrouchingShape_ : innerStandingShape_};
        player_->SetInnerBodyShape(inner_shape);
    }
}

void Player::toggleSprint(bool enable)
{
    if (characterSprinting_ == enable)
        return;

    characterSprinting_ = !characterSprinting_;
}

void Player::handleInput(const JPH::Vec3 &direction, bool jump, float deltaTime)
{
    using namespace JPH;

    const bool playerControlsHorizontalVelocity{controlMovementDuringJump_ || player_->IsSupported()};

    if (playerControlsHorizontalVelocity)
    {
        float speed{0.0f};

        if (characterSprinting_ && !crouched_)
            speed = characterSpeed_ * characterSprintSpeedMultiplier_;
        else
            speed = characterSpeed_;

        if (crouched_ && player_->IsSupported())
            speed *= characterCrouchSpeedMultiplier;

        // Smooth the player input
        desiredVelocity_ = 0.25f * direction * speed + 0.75f * desiredVelocity_;

        // True if the player intended to move
        allowSliding_ = !direction.IsNearZero();
    }
    else
    {
        // While in air we allow sliding
        allowSliding_ = true;
    }

    // Current velocity
    Vec3 currentVerticalVelocity{player_->GetLinearVelocity().Dot(player_->GetUp()) * player_->GetUp()};
    Vec3 groundVelocity{player_->GetGroundVelocity()};
    bool movingTowardsGround{(currentVerticalVelocity.GetY() - groundVelocity.GetY()) < 0.1f};

    Vec3 newVelocity;

    if (player_->GetGroundState() == CharacterVirtual::EGroundState::OnGround // If on ground
        && (enableCharacterInertia_
                ? movingTowardsGround // Inertia enabled: And not moving away from ground
                : !player_->IsSlopeTooSteep(
                      player_->GetGroundNormal()))) // Inertia disabled: And not on a slope that is too steep
    {
        // Assume velocity of ground when on ground
        newVelocity = groundVelocity;

        // Jump
        if (jump && movingTowardsGround)
            newVelocity += jumpSpeed_ * player_->GetUp();
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
        Vec3 currentHorizontalVelocity = player_->GetLinearVelocity() - currentVerticalVelocity;
        newVelocity += currentHorizontalVelocity;
    }

    // Update character velocity
    player_->SetLinearVelocity(newVelocity);
}

void Player::physicsUpdate(float deltaTime)
{
    using namespace JPH;

    // Move character, using default update settings
    CharacterVirtual::ExtendedUpdateSettings updateSettings;
    player_->ExtendedUpdate(deltaTime, physicsSystem_->GetGravity(), updateSettings,
                            physicsSystem_->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
                            physicsSystem_->GetDefaultLayerFilter(Layers::MOVING), {}, {}, *tempAllocator_);
}

void Player::OnContactSolve(const JPH::CharacterVirtual *inCharacter, const JPH::BodyID &inBodyID2,
                            const JPH::SubShapeID &inSubShapeID2, JPH::RVec3Arg inContactPosition,
                            JPH::Vec3Arg inContactNormal, JPH::Vec3Arg inContactVelocity,
                            const JPH::PhysicsMaterial *inContactMaterial, JPH::Vec3Arg inCharacterVelocity,
                            JPH::Vec3 &ioNewCharacterVelocity)
{
    // Ignore callbacks for other characters than the player
    if (inCharacter != player_)
        return;

    // Don't allow the player to slide down static not-too-steep surfaces when not actively moving and when not on a
    // moving platform
    if (!allowSliding_ && inContactVelocity.IsNearZero() && !inCharacter->IsSlopeTooSteep(inContactNormal))
        ioNewCharacterVelocity = JPH::Vec3::sZero();
}

JPH::Vec3 Player::getVelocity() const
{
    return player_->GetLinearVelocity();
}
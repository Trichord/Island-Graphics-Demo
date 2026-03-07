#include "jolt/Jolt.h"

#include "player.h"

#include <iostream>

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "jolt/Physics/Character/CharacterVirtual.h"
#include "jolt/Physics/PhysicsSystem.h"
#include "jolt_impl.h"
#include "world_vectors.h"
#include <jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

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

    // Create 'player' character
    Ref<CharacterVirtualSettings> settings = new CharacterVirtualSettings();
    settings->mMaxSlopeAngle = maxSlopeAngle_;
    settings->mShape = standingShape_;
    settings->mSupportingVolume =
        Plane(Vec3::sAxisY(), -characterRadiusStanding_); // Accept contacts that touch the lower sphere of the capsule
    settings->mEnhancedInternalEdgeRemoval = true;
    settings->mInnerBodyShape = innerStandingShape_;
    settings->mInnerBodyLayer = Layers::MOVING;
    player_ = new CharacterVirtual(settings, RVec3::sZero(), Quat::sIdentity(), 0, &physicsSystem);
}

//Returns the center of mass of the capsule
JPH::Vec3 Player::getCenter() const
{
    return player_->GetCenterOfMassPosition();
}

//Returns the bottom of the capsule, per JPH
JPH::Vec3 Player::getPosition() const
{
    return player_->GetPosition();
}

void Player::setPosition(JPH::Vec3 position)
{
    player_->SetPosition(position);
}

void Player::handleInput(const JPH::Vec3 &direction, bool jump, bool switchStance, float deltaTime)
{
    using namespace JPH;

    const bool playerControlsHorizontalVelocity{controlMovementDuringJump_ || player_->IsSupported()};

    if (playerControlsHorizontalVelocity)
    {
        // Smooth the player input
        desiredVelocity_ = 0.25f * direction * characterSpeed_ + 0.75f * desiredVelocity_;

        // True if the player intended to move
        allowSliding_ = !direction.IsNearZero();
    }
    else
    {
        // While in air we allow sliding
        allowSliding_ = true;
    }

    // Determine new basic velocity
    Vec3 currentVerticalVelocity{player_->GetLinearVelocity().Dot(player_->GetUp()) * player_->GetUp()};
    Vec3 ground_velocity{player_->GetGroundVelocity()};
    Vec3 newVelocity;
    bool movingTowardsGround{(currentVerticalVelocity.GetY() - ground_velocity.GetY()) < 0.1f};
    if (player_->GetGroundState() == CharacterVirtual::EGroundState::OnGround // If on ground
        && (enableCharacterInertia_
                ? movingTowardsGround // Inertia enabled: And not moving away from ground
                : !player_->IsSlopeTooSteep(
                      player_->GetGroundNormal()))) // Inertia disabled: And not on a slope that is too steep
    {
        // Assume velocity of ground when on ground
        newVelocity = ground_velocity;

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

    // Stance switch
    if (switchStance)
    {
        bool isStanding = player_->GetShape() == standingShape_;
        const Shape *shape = isStanding ? crouchingShape_ : standingShape_;
        if (player_->SetShape(shape, 1.5f * physicsSystem_->GetPhysicsSettings().mPenetrationSlop,
                              physicsSystem_->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
                              physicsSystem_->GetDefaultLayerFilter(Layers::MOVING), {}, {}, *tempAllocator_))
        {
            const Shape *inner_shape = isStanding ? innerCrouchingShape_ : innerStandingShape_;
            player_->SetInnerBodyShape(inner_shape);
        }
    }
}

void Player::prePhysicsUpdate(float deltaTime)
{
    using namespace JPH;

    // Move character
    CharacterVirtual::ExtendedUpdateSettings update_settings;
    player_->ExtendedUpdate(deltaTime, physicsSystem_->GetGravity(), update_settings,
                            physicsSystem_->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
                            physicsSystem_->GetDefaultLayerFilter(Layers::MOVING), {}, {}, *tempAllocator_);
}
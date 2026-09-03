#include "physics/character_capsule.h"

#include <Jolt/Core/Core.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Geometry/Plane.h>
#include <Jolt/Math/MathTypes.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Character/CharacterBase.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SubShapeID.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <optional>

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

CharacterCapsule::CharacterCapsule(JPH::PhysicsSystem &physicsSystem, JPH::TempAllocatorImpl &tempAllocator,
                                   Settings capsuleSettings, JPH::uint64 userID)
    : physicsSystem_{&physicsSystem}, tempAllocator_{&tempAllocator}, capsuleSettings_{capsuleSettings},
      standingShape_{createShape(capsuleSettings.standingHeight, capsuleSettings.radius)},
      crouchingShape_{createShape(capsuleSettings.crouchingHeight, capsuleSettings.radius)},
      innerStandingShape_{createShape(capsuleSettings.innerShapeFraction * capsuleSettings.standingHeight,
                                      capsuleSettings.innerShapeFraction * capsuleSettings.radius)},
      innerCrouchingShape_{createShape(capsuleSettings.innerShapeFraction * capsuleSettings.crouchingHeight,
                                       capsuleSettings.innerShapeFraction * capsuleSettings.radius)}
{
    using namespace JPH;

    Ref<CharacterVirtualSettings> settings = new CharacterVirtualSettings();
    settings->mMaxSlopeAngle = maxSlopeAngle_;
    settings->mShape = standingShape_;
    settings->mSupportingVolume =
        Plane(Vec3::sAxisY(), -capsuleSettings_.radius); // Accept contacts that touch the lower sphere of the capsule
    settings->mEnhancedInternalEdgeRemoval = true;
    settings->mInnerBodyShape = innerStandingShape_;
    settings->mInnerBodyLayer = Layers::MOVING;

    // Create the player's capsule
    capsule_ = new CharacterVirtual(settings, RVec3::sZero(), Quat::sIdentity(), userID, &physicsSystem);
    capsule_->SetListener(this);
}

float CharacterCapsule::getRadius() const
{
    return capsuleSettings_.radius;
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

void CharacterCapsule::setCenter(const JPH::Vec3 &center)
{
    // Distance from bottom of capsule to center
    JPH::Vec3 offset{capsule_->GetCenterOfMassPosition() - capsule_->GetPosition()};
    capsule_->SetPosition(center - offset);
}

void CharacterCapsule::setPosition(const JPH::Vec3 &position)
{
    capsule_->SetPosition(position);
}

void CharacterCapsule::setVelocity(const JPH::Vec3 &velocity)
{
    capsule_->SetLinearVelocity(velocity);
    desiredVelocity_ = JPH::Vec3::sZero();
}

void CharacterCapsule::toggleCrouch(bool enable)
{
    using namespace JPH;

    if (crouched_ == enable)
        return;

    // Toggle crouch based off the current shape of the capsule
    bool isStanding{capsule_->GetShape() == standingShape_};
    const Shape *shape{isStanding ? crouchingShape_ : standingShape_};

    if (capsule_->SetShape(shape, 1.5f * physicsSystem_->GetPhysicsSettings().mPenetrationSlop,
                           physicsSystem_->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
                           physicsSystem_->GetDefaultLayerFilter(Layers::MOVING), {}, {}, *tempAllocator_))
    {
        const Shape *inner_shape{isStanding ? innerCrouchingShape_ : innerStandingShape_};
        capsule_->SetInnerBodyShape(inner_shape);
        crouched_ = enable;
    }
}

void CharacterCapsule::toggleSprint(bool enable)
{
    if (characterSprinting_ == enable)
        return;

    // Stop sprint toggle mid-air, since that affects velocity.
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
            speed = capsuleSettings_.speed * capsuleSettings_.sprintSpeedMultiplier;
        else
            speed = capsuleSettings_.speed;

        if (crouched_ && capsule_->IsSupported())
            speed *= capsuleSettings_.crouchSpeedMultiplier;

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

    // New velocity must start at 0
    Vec3 newVelocity{JPH::Vec3::sZero()};
    Vec3 currentVerticalVelocity{capsule_->GetLinearVelocity().Dot(capsule_->GetUp()) * capsule_->GetUp()};
    bool movingTowardsGround{currentVerticalVelocity.GetY() < 0.1f};

    if (isGrounded() && movingTowardsGround)
    {
        // Leave new velocity zero'd unless jumping
        if (jump)
            newVelocity = capsuleSettings_.jumpSpeed * capsule_->GetUp();
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
    CharacterVirtual::ExtendedUpdateSettings updateSettings{.mWalkStairsStepUp = Vec3{0.0f, 0.1f, 0.0f}};
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

    // Don't allow the player to slide down static not-too-steep surfaces when not actively moving and when not on a
    // moving platform
    if (!allowSliding_ && inContactVelocity.IsNearZero() && !inCharacter->IsSlopeTooSteep(inContactNormal))
        ioNewCharacterVelocity = JPH::Vec3::sZero();

    // Cancel out any vertical component to prevent steep slope jitter
    if (inCharacter->IsSlopeTooSteep(inContactNormal))
    {
        JPH::Vec3 up{inCharacter->GetUp()};
        float vertical{ioNewCharacterVelocity.Dot(up)};
        if (vertical > 0.0f)
            ioNewCharacterVelocity -= vertical * up;
    }
}

bool CharacterCapsule::OnContactValidate(const JPH::CharacterVirtual *inCharacter,
                                         const JPH::CharacterContact &inContact)
{
    // Prevent standing on upside down faces by checking if surface normal is within ~30 degrees of up.
    return !inContact.mIsBackFacingContact || inContact.mSurfaceNormal.Dot(inCharacter->GetUp()) < 0.866025f;
}

JPH::Vec3 CharacterCapsule::getVelocity() const
{
    return capsule_->GetLinearVelocity();
}

JPH::BodyID CharacterCapsule::getBodyID() const
{
    return capsule_->GetInnerBodyID();
}

bool CharacterCapsule::isSupported() const
{
    return capsule_->IsSupported();
}

bool CharacterCapsule::isGrounded() const
{
    return capsule_->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
}

std::optional<float> CharacterCapsule::predictImpactSpeed(float lookAheadTime) const
{
    using namespace JPH;

    if (isSupported())
        return std::nullopt;

    const Vec3 velocity{capsule_->GetLinearVelocity()};
    const Vec3 gravity{physicsSystem_->GetGravity()};
    const Vec3 up{capsule_->GetUp()};
    const Vec3 sweep{velocity * lookAheadTime + 0.5f * gravity * lookAheadTime * lookAheadTime};

    // Not falling
    if (sweep.Dot(up) >= 0.0f)
        return std::nullopt;

    RShapeCast shapeCast{
        RShapeCast::sFromWorldTransform(capsule_->GetShape(), Vec3::sOne(), capsule_->GetWorldTransform(), sweep)};

    ShapeCastSettings settings;
    ClosestHitCollisionCollector<CastShapeCollector> collector;
    physicsSystem_->GetNarrowPhaseQuery().CastShape(
        shapeCast, settings, shapeCast.mCenterOfMassStart.GetTranslation(), collector,
        physicsSystem_->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
        physicsSystem_->GetDefaultLayerFilter(Layers::MOVING), IgnoreSingleBodyFilter{capsule_->GetInnerBodyID()});

    if (!collector.HadHit())
        return std::nullopt;

    const Vec3 normal{-collector.mHit.mPenetrationAxis.Normalized()};

    // Not a true landing, capsule will be sliding and bouncing down the slope
    if (capsule_->IsSlopeTooSteep(normal))
        return std::nullopt;

    // Approximate time to impact
    const float timeToImpact{collector.mHit.mFraction * lookAheadTime};
    const Vec3 impactVelocity{velocity + gravity * timeToImpact};
    const float impactSpeed{-impactVelocity.Dot(normal)};

    if (impactSpeed > 0.0f)
        return impactSpeed;

    return std::nullopt;
}

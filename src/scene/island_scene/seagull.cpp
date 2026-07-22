#include "scene/island_scene/seagull.h"

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/gtx/compatibility.hpp>

namespace
{
constexpr float epsilon{0.05f};
}

Seagull::Seagull(const Transform &startingTransform, Seagull::State state, float panicDistance, float orbitHeight,
                 float orbitTime, float orbitRadius, float restTime)
    : state_{state}, orbitHeight_{orbitHeight}, orbitTime_{orbitTime}, orbitRadius_{orbitRadius}, restTime_{restTime},
      panicDist_{panicDistance}, startingPos_{startingTransform.getPosition()}, transform_{startingTransform}
{
}

Seagull::State Seagull::getState() const
{
    return state_;
}

const glm::vec3 &Seagull::getPosition() const
{
    return transform_.getPosition();
}

const glm::quat &Seagull::getRotation() const
{
    return transform_.getRotation();
}

bool Seagull::stateChanged()
{
    if (hasChanged_)
    {
        hasChanged_ = false;
        return true;
    }

    return false;
}

void Seagull::update(const glm::vec3 &playerPos, float time, float deltaTime)
{
    updateState(playerPos, deltaTime);
    updatePosition(playerPos, time, deltaTime);
}

// Update the state of the seagull. Deliberately allows transitions through multiple states in one frame if given bad
// inputs
void Seagull::updateState(const glm::vec3 &playerPos, float deltaTime)
{
    constexpr float returnPositionEpsilon{0.1f};
    using enum Seagull::State;

    const glm::vec3 &position{transform_.getPosition()};
    const bool playerTooClose{glm::length(position - playerPos) <= panicDist_};

    if (state_ == resting && playerTooClose && restTimer_ >= restTime_)
    {
        state_ = panicking;
        restTimer_ = 0.0f;
        hasChanged_ = true;
    }

    if (state_ == panicking && position.y >= orbitHeight_)
    {
        state_ = orbiting;
        hasChanged_ = true;
        const glm::vec3 &pos{transform_.getPosition()};
        const glm::vec3 front{transform_.getFront()};
        const glm::vec3 flatFront{glm::normalize(glm::vec3{front.x, 0.0f, front.z})};
        const glm::vec3 perpendicular{-flatFront.z, 0.0f, flatFront.x};

        flipOrbit_ = static_cast<int>(position.z) % 2 == 0;
        orbitPos_ = pos + (flipOrbit_ ? perpendicular : -perpendicular) * orbitRadius_;
        orbitAngle_ = glm::atan2(pos.z - orbitPos_.z, pos.x - orbitPos_.x);
    }

    if (state_ == orbiting && !playerTooClose)
        orbitTimer_ += deltaTime;

    if (state_ == orbiting && orbitTimer_ >= orbitTime_)
    {
        orbitTimer_ = 0.0f;
        state_ = returning;
        hasChanged_ = true;
    }

    if (state_ == returning && playerTooClose)
    {
        state_ = panicking;
        hasChanged_ = true;
    }

    if (state_ == returning && glm::distance(position, startingPos_) <= returnPositionEpsilon)
    {
        state_ = resting;
        hasChanged_ = true;
    }

    if (state_ == resting)
        restTimer_ += deltaTime;
}

void Seagull::updatePosition(const glm::vec3 &playerPos, float time, float deltaTime)
{
    using enum Seagull::State;

    const glm::vec3 &position{transform_.getPosition()};
    const glm::vec3 direction{transform_.getFront()};
    float pitch{transform_.getPitch()};

    switch (state_)
    {
    case panicking: {
        const float pitchBlend{(position.y - startingPos_.y) / (orbitHeight_ - startingPos_.y)};
        pitch = climbAngle_ * rampPitch(pitchBlend);

        const float yaw{glm::atan2(-direction.x, -direction.z)};
        const glm::quat yawQuat{glm::angleAxis(yaw, WorldVectors::worldUp)};
        const glm::quat pitchQuat{glm::angleAxis(glm::radians(pitch), WorldVectors::worldRight)};

        transform_.setAbsoluteRotation(yawQuat * pitchQuat);
        transform_.setRelativeTranslation(transform_.getFront() * flySpeed_ * deltaTime);

        return;
    }

    case orbiting: {
        const float angularSpeed{flySpeed_ / orbitRadius_};
        orbitAngle_ += (flipOrbit_ ? angularSpeed : -angularSpeed) * deltaTime;

        const glm::vec3 newPos{orbitPos_ + glm::vec3{glm::cos(orbitAngle_), 0.0f, sin(orbitAngle_)} * orbitRadius_};
        const glm::vec3 heading{glm::normalize(newPos - position)};
        const float yaw{glm::atan2(-heading.x, -heading.z)};

        transform_.setAbsoluteRotation(glm::angleAxis(yaw, WorldVectors::worldUp));
        transform_.setAbsoluteTranslation(newPos);

        return;
    }

    case returning: {
        const float pitchBlend{(position.y - startingPos_.y) / (orbitHeight_ - startingPos_.y)};
        pitch = -climbAngle_ * rampPitch(pitchBlend);

        glm::vec3 home{glm::normalize(startingPos_ - position)};
        glm::vec3 heading{
            glm::mix(direction, home, 1.0f - glm::smoothstep(1.0f - transitionZone_, 1.0f + epsilon, pitchBlend))};

        const float yaw{glm::atan2(-heading.x, -heading.z)};
        const glm::quat yawQuat{glm::angleAxis(yaw, WorldVectors::worldUp)};
        const glm::quat pitchQuat{glm::angleAxis(glm::radians(pitch), WorldVectors::worldRight)};

        transform_.setAbsoluteRotation(yawQuat * pitchQuat);
        transform_.setRelativeTranslation(transform_.getFront() * flySpeed_ * deltaTime);
    }
    }
}

float Seagull::rampPitch(float blend) const
{
    // Smooth out takeoff and landing transitions otherwise the bird snaps into place
    if (blend <= transitionZone_)
        return glm::smoothstep(-epsilon, transitionZone_, blend);
    else if (blend >= 1.0f - transitionZone_)
        return 1.0f - glm::smoothstep(1.0f - transitionZone_, 1.0f + epsilon, blend);
    else
        return 1.0f;
}
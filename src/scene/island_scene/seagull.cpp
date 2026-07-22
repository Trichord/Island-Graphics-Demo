#include "scene/island_scene/seagull.h"

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include "utilities/transform.h"
#include "utilities/world_vectors.h"

Seagull::Seagull(const Transform &startingTransform, Seagull::State state, float panicDistance, float orbitHeight,
                 float orbitTime, float orbitRadius, float restTime)
    : state_{state}, orbitHeight_{orbitHeight}, orbitTime_{orbitTime}, orbitRadius_{orbitRadius}, restTime_{restTime},
      panicDist_{panicDistance}, homePos_{startingTransform.getPosition()}, transform_{startingTransform}
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
    if (stateChanged_)
    {
        stateChanged_ = false;
        return true;
    }

    return false;
}

void Seagull::update(const glm::vec3 &playerPos, float deltaTime)
{
    updateState(playerPos, deltaTime);
    updatePosition(deltaTime);
}

bool Seagull::canReturnHome() const
{
    const glm::vec3 home{homePos_ - transform_.getPosition()};
    const glm::vec3 &direction{transform_.getFront()};
    const glm::vec2 flatHome{home.x, home.z};

    // If the bird is within ~22.5 degrees of the home direction, its safe to break the circle
    return glm::dot(glm::normalize(glm::vec2{direction.x, direction.z}), glm::normalize(flatHome)) > 0.92388f;
}

void Seagull::orbit(float deltaTime)
{
    const float angularSpeed{flySpeed_ / orbitRadius_};
    orbitAngle_ += (flipOrbit_ ? angularSpeed : -angularSpeed) * deltaTime;

    const glm::vec3 newPos{orbitPos_ + glm::vec3{glm::cos(orbitAngle_), 0.0f, glm::sin(orbitAngle_)} * orbitRadius_};
    const glm::vec3 heading{glm::normalize(newPos - transform_.getPosition())};
    const float yaw{glm::atan2(-heading.x, -heading.z)};

    transform_.setAbsoluteRotation(glm::angleAxis(yaw, WorldVectors::worldUp));
    transform_.setAbsoluteTranslation(newPos);
}

void Seagull::updateFlight(float deltaTime)
{
    flightTimer_ += deltaTime;

    const float mixFactor{glm::clamp(flightTimer_ / flightDuration_, 0.0f, 1.0f)};
    glm::vec3 newPos;
    newPos.x = glm::mix(startingPoint_.x, endingPoint_.x, mixFactor);
    newPos.z = glm::mix(startingPoint_.z, endingPoint_.z, mixFactor);
    newPos.y = glm::mix(startingPoint_.y, endingPoint_.y, glm::smoothstep(0.0f, 1.0f, mixFactor));

    transform_.setAbsoluteTranslation(newPos);

    const float decentRate{flightHeight_ * 6.0f * mixFactor * (1.0f - mixFactor)};
    const float pitch{glm::atan2(decentRate, flightFlatDist_)};
    const glm::quat yawQuat{glm::angleAxis(flightYaw_, WorldVectors::worldUp)};
    const glm::quat pitchQuat{glm::angleAxis(pitch, WorldVectors::worldRight)};

    transform_.setAbsoluteRotation(yawQuat * pitchQuat);
}

void Seagull::updatePosition(float deltaTime)
{
    using enum Seagull::State;

    switch (state_)
    {
    case panicking:
        updateFlight(deltaTime);
        return;

    case orbiting:
        orbit(deltaTime);
        return;

    case returning:
        updateFlight(deltaTime);
        return;

    case resting:
        return;
    }
}

void Seagull::setFlight(const glm::vec3 &start, const glm::vec3 &end)
{
    flightTimer_ = 0.0f;
    startingPoint_ = start;
    endingPoint_ = end;

    const glm::vec3 flightDir{end - start};
    flightFlatDist_ = glm::length(glm::vec2{flightDir.x, flightDir.z});
    flightDuration_ = flightFlatDist_ / flySpeed_;
    flightYaw_ = glm::atan2(-flightDir.x, -flightDir.z);
    flightHeight_ = flightDir.y;
}

// Update the state of the seagull. Deliberately allows transitions through multiple states in one frame if given bad
// inputs
void Seagull::updateState(const glm::vec3 &playerPos, float deltaTime)
{
    constexpr float returnPositionEpsilon{0.15f};
    using enum Seagull::State;

    auto createDestination = [](const glm::vec3 &position, const glm::vec3 &flatFront, float height, float angle) {
        float dist{(height - position.y) / glm::tan(angle)};
        return glm::vec3{position.x + flatFront.x * dist, height, position.z + flatFront.z * dist};
    };

    const glm::vec3 &position{transform_.getPosition()};
    const glm::vec3 front{transform_.getFront()};
    const glm::vec3 flatFront{glm::normalize(glm::vec3{front.x, 0.0f, front.z})};
    const bool playerTooClose{glm::length(position - playerPos) <= panicDist_};

    if (state_ == resting && playerTooClose && restTimer_ >= restTime_)
    {
        state_ = panicking;
        restTimer_ = 0.0f;
        setFlight(homePos_, createDestination(homePos_, flatFront, orbitHeight_, climbAngle_));
        stateChanged_ = true;
    }

    if (state_ == panicking && position.y >= orbitHeight_)
    {
        state_ = orbiting;

        const glm::vec3 perpendicular{-flatFront.z, 0.0f, flatFront.x};
        flipOrbit_ = static_cast<int>(position.z * 10.0f) % 2 == 0;
        orbitPos_ = position + (flipOrbit_ ? perpendicular : -perpendicular) * orbitRadius_;
        orbitAngle_ = glm::atan2(position.z - orbitPos_.z, position.x - orbitPos_.x);
        stateChanged_ = true;
    }

    if (state_ == orbiting && !playerTooClose)
        orbitTimer_ += deltaTime;

    if (state_ == orbiting && orbitTimer_ >= orbitTime_ && canReturnHome())
    {
        orbitTimer_ = 0.0f;
        state_ = returning;
        setFlight(position, homePos_);
        stateChanged_ = true;
    }

    if (state_ == returning && playerTooClose)
    {
        state_ = panicking;
        float dist{(orbitHeight_ - position.y) / glm::tan(climbAngle_)};
        setFlight(position, createDestination(position, flatFront, orbitHeight_, climbAngle_));
        stateChanged_ = true;
    }

    if (state_ == returning && glm::length(homePos_ - position) <= returnPositionEpsilon)
    {
        state_ = resting;
        transform_.setAbsoluteTranslation(homePos_);
        stateChanged_ = true;
    }

    if (state_ == resting)
        restTimer_ += deltaTime;
}
#pragma once

#include <glm/glm.hpp>

#include "scene/scene.h"
#include "utilities/transform.h"

class Seagull
{
  public:
    enum class State
    {
        resting,
        panicking,
        orbiting,
        returning
    };

    Seagull() = default;
    Seagull(const Transform& startingTransform, Seagull::State state, float panicDistance, float orbitHeight, float orbitTime, float orbitRadius, float restTime);

    Seagull::State getState() const;
    const glm::vec3 &getPosition() const;
    const glm::quat &getRotation() const;

    void update(const glm::vec3 &playerPos, float time, float deltaTime);
    bool stateChanged();

  private:
    static constexpr float flySpeed_{3.0f};
    static constexpr float climbAngle_{20.0f};
    static constexpr float transitionZone_{0.15f};

    Seagull::State state_{Seagull::State::resting};

    float orbitHeight_{0.0f};
    float orbitTime_{0.0f};
    float orbitRadius_{0.0f};
    float orbitAngle_{0.0f};
    bool flipOrbit_{false};

    float restTime_{0.0f};
    float panicDist_{0.0f};

    float orbitTimer_{0.0f};
    float restTimer_{0.0f};

    bool hasChanged_{false};

    glm::vec3 startingPos_{0.0f};
    glm::vec3 orbitPos_{0.0f};
    Transform transform_;

    void updateState(const glm::vec3 &playerPos, float deltaTime);
    void updatePosition(const glm::vec3 &playerPos, float time, float deltaTime);
    float rampPitch(float blend) const;
};
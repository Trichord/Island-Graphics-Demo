#pragma once

#include <glm/glm.hpp>

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
    Seagull(const Transform &startingTransform, Seagull::State state, float panicDistance, float orbitHeight,
            float orbitTime, float orbitRadius, float restTime);

    Seagull::State getState() const;
    const glm::vec3 &getPosition() const;
    const glm::quat &getRotation() const;

    bool stateChanged();
    void update(const glm::vec3 &playerPos, float deltaTime);

  private:
    static constexpr float flySpeed_{3.0f};
    static constexpr float climbAngle_{glm::radians(20.0f)};

    Seagull::State state_{Seagull::State::resting};

    float orbitHeight_{0.0f};
    float orbitTime_{0.0f};
    float orbitRadius_{0.0f};
    float orbitAngle_{0.0f};
    bool flipOrbit_{false};

    float restTime_{0.0f};
    float panicDist_{0.0f};

    float flightDuration_{0.0f};
    float flightYaw_{0.0f};
    float flightFlatDist_{0.0f};
    float flightHeight_{0.0f};

    float orbitTimer_{0.0f};
    float flightTimer_{0.0f};
    float restTimer_{0.0f};

    bool stateChanged_{false};

    glm::vec3 homePos_{0.0f};
    glm::vec3 orbitPos_{0.0f};
    glm::vec3 startingPoint_{0.0f};
    glm::vec3 endingPoint_{0.0f};
    Transform transform_;

    bool canReturnHome() const;
    void orbit(float deltaTime);
    void updateFlight(float deltaTime);
    void updatePosition(float deltaTime);
    void setFlight(const glm::vec3 &start, const glm::vec3 &end);
    void updateState(const glm::vec3 &playerPos, float deltaTime);
};
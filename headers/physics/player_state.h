#pragma once

#include <glm/glm.hpp>

struct PlayerState
{
    glm::vec3 position{0.0f};
    glm::vec3 front{0.0f};
    glm::vec3 velocity{0.0f};

    bool supported{false};
    bool grounded{false};
    bool jumpTriggered{false};
    bool landingTriggered{false};
    bool footstepTriggered{false};
    bool running{false};
    bool crouched{false};
};
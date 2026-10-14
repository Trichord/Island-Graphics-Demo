#pragma once

#include <cstddef>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "scene/handle.h"

struct MovedSceneObject
{
    Handle handle{0};
    std::size_t transformIndex{0};
    glm::vec3 position{0.0f};
    glm::quat rotation{glm::identity<glm::quat>()};
};
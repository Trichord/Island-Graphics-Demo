#pragma once

#include "aabb.h"
#include "glm/glm.hpp"

struct InstancedData
{
    glm::mat4 modelMatrix{1.0f};
    glm::vec3 position{0.0f};
    AABB worldAABB;
};
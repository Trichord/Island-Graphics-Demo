#pragma once

#include "aabb.h"
#include "glm/glm.hpp"

struct InstanceData
{
    glm::mat4 modelMatrix{1.0f};
    glm::mat4 normalMatrix{1.0f};
    glm::vec3 position{0.0f};
    AABB worldAABB;
    bool isVisible{false};
    double sortKey{0.0f};
};
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <optional>
#include <unordered_map>
#include <vector>

struct Skeleton
{
    std::vector<std::size_t> jointNodes;
    std::vector<glm::mat4> invBindMatrices;

    Skeleton() = default;
    Skeleton(std::size_t numJoints)
    {
        invBindMatrices.resize(numJoints, glm::mat4{1.0f});
    }
};
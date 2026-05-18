#pragma once

#include "glm/glm.hpp"

enum class WorldOrientation
{
    UP,
    DOWN,
    RIGHT,
    LEFT,
    FRONT,
    BACK
};

namespace WorldVectors
{
constexpr glm::vec3 worldUp{0.0f, 1.0f, 0.0f};
constexpr glm::vec3 worldDown{-worldUp};

constexpr glm::vec3 worldRight{1.0f, 0.0f, 0.0f};
constexpr glm::vec3 worldLeft{-worldRight};

constexpr glm::vec3 worldFront{0.0f, 0.0f, -1.0f};
constexpr glm::vec3 worldBack{-worldFront};
}; // namespace WorldVectors
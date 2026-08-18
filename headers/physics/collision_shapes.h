#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <variant>

namespace Collision
{
struct Mesh
{
    std::filesystem::path path;
};

struct Capsule
{
    float radius{0.0f};
    float height{0.0f};
    glm::vec3 offset{0.0f};
};

using Geometry = std::variant<Mesh, Capsule>;
} // namespace Collision
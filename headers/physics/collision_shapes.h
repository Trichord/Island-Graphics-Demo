#pragma once

#include <filesystem>
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
};

using Geometry = std::variant<Mesh, Capsule>;
} // namespace Collision
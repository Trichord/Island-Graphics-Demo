#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <variant>

namespace collision
{
struct MaterialProperties
{
    // Good default, Jolt's 0.2f is a too slippery
    float friction{0.5f};

    // Matches Jolt's default density, quite heavy / hard to push around
    float density{1000.f};

    // Slightly higher than neutral, lightly floats
    float buoyancy{1.1f};
};

struct Mesh
{
    std::filesystem::path path;
    MaterialProperties material;
};

struct ConvexHull
{
    std::filesystem::path path;
    MaterialProperties material;
    bool compound{false};
    bool dynamic{false};
};

struct Capsule
{
    float radius{0.0f};
    float height{0.0f};
    glm::vec3 offset{0.0f};
    MaterialProperties material;
    bool dynamic{false};
};

struct Sphere
{
    float radius{0.0f};
    glm::vec3 offset{0.0f};
    float bounciness{0.0f};
    MaterialProperties material;
    bool dynamic{false};
};

using Geometry = std::variant<Mesh, ConvexHull, Capsule, Sphere>;
} // namespace collision
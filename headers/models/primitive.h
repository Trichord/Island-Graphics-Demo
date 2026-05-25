#pragma once

#include <cstdint>
#include <fastgltf/types.hpp>
#include <glm/glm.hpp>
#include <vector>

#include "models/aabb.h"

struct Primitive
{
    struct Vertex
    {
        glm::vec3 position{0.0f};
        glm::vec3 normal{0.0f};
        glm::vec4 tangent{0.0f};
        glm::vec2 texCoords{0.0f};
    };

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    AABB aabb;
    int materialIndex{0};
    fastgltf::AlphaMode alphaMode{fastgltf::AlphaMode::Opaque};
};
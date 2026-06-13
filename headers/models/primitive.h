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
        std::uint32_t normal{0};
        std::uint32_t tangent{0};
        std::uint32_t texCoords{0};
        std::uint32_t boneDataIndex{0xFFFFFFFF};
        std::uint32_t color{};
    };

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    AABB aabb;
    glm::mat4 nodeTransform{1.0f};
    std::size_t materialIndex{0};
    fastgltf::AlphaMode alphaMode{fastgltf::AlphaMode::Opaque};
};
#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <vector>

#include "models/aabb.h"
#include "models/animation/animation.h"
#include "models/animation/skeleton.h"

enum class AlphaMode : std::uint8_t
{
    opaque,
    mask,
    blend
};

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

    struct VertexSkin
    {
        glm::vec4 weights{0.0f};
        glm::uvec4 boneIDs{0u};
    };

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::optional<std::vector<VertexSkin>> vertexSkins;

    AABB aabb;
    glm::mat4 modelSpaceTransform{1.0f};
    std::size_t materialIndex{0};
    AlphaMode alphaMode{AlphaMode::opaque};
};
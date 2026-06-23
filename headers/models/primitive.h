#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <vector>

#include "models/aabb.h"
#include "models/animation.h"
#include "models/skeleton.h"

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

    struct VertexSkinning
    {
        std::vector<glm::vec4> weights;
        std::vector<glm::uvec4> boneIDs;
    };

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::optional<VertexSkinning> skinning;

    AABB aabb;
    glm::mat4 modelSpaceTransform{1.0f};
    std::size_t materialIndex{0};
    AlphaMode alphaMode{AlphaMode::opaque};
};
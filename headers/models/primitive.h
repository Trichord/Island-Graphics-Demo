#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <vector>

#include "models/aabb.h"
#include "models/animation/animation.h"
#include "models/animation/skeleton.h"
#include "renderer/shader/graphics_shader.h"

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
        std::uint32_t normal{0x7fc00}; // (0, 1, 0, 0) a.k.a WorldVectors::worldUp
        std::uint32_t tangent{0x5ff00000}; // (0, 0, 1, 1)
        std::uint32_t texCoords{0};
        std::uint32_t vboIndex{0};
        std::uint32_t color{0xff000000}; // (0, 0, 0, 1)
    };

    struct VertexSkin
    {
        glm::vec4 weights{0.0f};
        glm::uvec4 boneIDs{0u};
    };

    struct VertexExplosion
    {
        float minDistance{0.0f};
        float maxDistance{0.0f};
    };

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::optional<std::vector<VertexSkin>> vertexSkins{std::nullopt};
    std::optional<VertexExplosion> explosionDistances{std::nullopt};

    AABB aabb;
    glm::mat4 modelSpaceTransform{1.0f};
    std::size_t materialIndex{0};
    AlphaMode alphaMode{AlphaMode::opaque};
    ShaderType shaderType{ShaderType::pbr};
};
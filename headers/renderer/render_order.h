#pragma once

#include <cassert>
#include <cstdint>

#include "renderer/shader/graphics_shader.h"

// Renderer will order draw buckets according to this enum. Renderer's shader array is sized to count.
enum RenderOrder : std::uint8_t
{
    baseColor,
    pbr,
    reflectiveCubemap,
    vegetation,
    particles,
    ocean,
    skybox,
    water, // Blended, after all opaque
    count,
};

constexpr RenderOrder toRenderOrder(ShaderType shader)
{
    using enum ShaderType;

    switch (shader)
    {
    case baseColor:
        return RenderOrder::baseColor;
    case pbr:
        return RenderOrder::pbr;
    case reflectiveCubemap:
        return RenderOrder::reflectiveCubemap;
    case vegetation:
        return RenderOrder::vegetation;
    case particles:
        return RenderOrder::particles;
    case ocean:
        return RenderOrder::ocean;
    case skybox:
        return RenderOrder::skybox;
    case water:
        return RenderOrder::water;

    case trees: // split into pbr + vegetation at load time
        break;
    }

    assert(false && "ERROR: shader has no render bucket");
    return RenderOrder::count;
}

constexpr std::uint8_t shaderIndex(RenderOrder order)
{
    assert(order < RenderOrder::count);
    return static_cast<std::uint8_t>(order);
}
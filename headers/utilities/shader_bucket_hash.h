#pragma once

#include <fastgltf/types.hpp>
#include <type_traits>
#include <utility>

#include "renderer/shader/graphics_shader.h"

using ShaderBucket = std::pair<ShaderType, fastgltf::AlphaMode>;

struct ShaderBucketHash
{
    std::size_t operator()(const ShaderBucket &b) const
    {
        auto h1 = std::hash<int>{}(static_cast<int>(b.first));
        auto h2 = std::hash<int>{}(static_cast<int>(b.second));

        return h1 ^ (h2 << 1);
    }
};
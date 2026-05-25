#pragma once

#include <array>
#include <cstdint>
#include <glad/glad.h>

struct Sampler
{
    std::uint16_t magFilter{GL_LINEAR};
    std::uint16_t minFilter{GL_LINEAR_MIPMAP_LINEAR};
    std::uint16_t wrapS{GL_REPEAT};
    std::uint16_t wrapT{GL_REPEAT};
};

struct SamplerArrayIndex
{
    int array{-1};
    int layer{-1};
};

struct Texture
{
    GLuint id{0};
    int width{0};
    int height{0};
    GLenum internalFormat{GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM};

    SamplerArrayIndex arrayIndex;
};

struct Material
{
    enum Type
    {
        albedo,
        ARM,
        normal,
        cubemap,
        count
    };

    std::array<SamplerArrayIndex, Type::count> layers;

    SamplerArrayIndex &operator[](Type t)
    {
        return layers[t];
    }

    const SamplerArrayIndex &operator[](Type t) const
    {
        return layers[t];
    }
};

#pragma once

#include <array>
#include <cstdint>
#include <glad/glad.h>

// Texture unit bindings
namespace TextureUnit
{
static constexpr std::uint32_t array2DBase{0};
static constexpr std::uint32_t arrayCubemapBase{12};
static constexpr std::uint32_t shadow{15};
static constexpr std::uint32_t resolvedColor{16};
static constexpr std::uint32_t depthMap{17};
static constexpr std::uint32_t hiZ{18};
static constexpr std::uint32_t noise{19};
static constexpr std::uint32_t max{32};
} // namespace TextureUnit

namespace ImageUnit
{
static constexpr std::uint32_t hiZRead{0};
static constexpr std::uint32_t hiZWrite{1};
static constexpr std::uint32_t max{8};
}

namespace DynamicCubemaps
{
constexpr GLenum internalFormat{GL_SRGB8_ALPHA8};
}

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
    enum Type : std::uint8_t
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
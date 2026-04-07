#pragma once

// OpenGL texture units are bound to this enum
enum class TextureUnit
{
    arrayRGB4k,
    arrayRGBA4k,
    arrayRGB2k,
    arrayRGBA2k,
    arrayRGB1k,
    arrayRGBA1k,
    arrayRGB512,
    arrayRGB256,
    cubemap,
    shadow
};

struct Sampler
{
    std::uint16_t magFilter{GL_LINEAR};
    std::uint16_t minFilter{GL_LINEAR_MIPMAP_LINEAR};
    std::uint16_t wrapS{GL_REPEAT};
    std::uint16_t wrapT{GL_REPEAT};
};

struct SamplerArrayEntry
{
    int index{-1};
    int depth{-1};
};

struct Texture
{
    int width{0};
    int height{0};
    GLenum internalFormat{GL_SRGB8};
    GLuint id{0};
    SamplerArrayEntry arrayEntry;
};

struct Material
{
    SamplerArrayEntry albedo;
    SamplerArrayEntry ARM;
    SamplerArrayEntry normal;
    GLuint cubemap{0};
};

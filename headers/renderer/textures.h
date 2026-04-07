#pragma once

// OpenGL texture units are bound to this enum
enum class TextureUnit
{
    arrayRGB4k,
    arraySRGB4k,
    arraySRGBA4k,
    arrayRGB2k,
    arraySRGB2k,
    arraySRGBA2k,
    arrayRGB1k,
    arraySRGB1k,
    arraySRGBA1k,
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
    int array{-1};
    int layer{-1};
};

struct Texture
{
    GLuint id{0};

    int width{0};
    int height{0};
    GLenum internalFormat{GL_SRGB8};
    
    SamplerArrayEntry arrayEntry;
};

struct Material
{
    SamplerArrayEntry albedo;
    SamplerArrayEntry ARM;
    SamplerArrayEntry normal;
    GLuint cubemap{0};
};
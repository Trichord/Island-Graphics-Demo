#pragma once

#include <cstdint>
#include <vector>

#include "glad/glad.h"
#include "image_loader.h"

// magFilter, minFilter, wrapS, wrapT
struct Sampler
{
    std::uint16_t magFilter, minFilter, wrapS, wrapT;

    constexpr Sampler(uint16_t mag = GL_LINEAR, uint16_t min = GL_LINEAR_MIPMAP_LINEAR, uint16_t ws = GL_REPEAT,
                      uint16_t wt = GL_REPEAT)
        : magFilter(mag), minFilter(min), wrapS(ws), wrapT(wt) {};
};

// Request sRGB, request mipmaps, degree of anisotropy
struct TextureDetails
{
    bool sRGBRequested{false};
    bool mipMapsRequested{false};
    float anisotropic{0.0f};

    constexpr TextureDetails() = default;
    constexpr TextureDetails(bool requestSRGB, bool requestMipmaps, float ansiotropicDegree)
        : sRGBRequested{requestSRGB}, mipMapsRequested{requestMipmaps}, anisotropic{ansiotropicDegree}
    {
    }
};

namespace textureUtilities
{
GLuint generateTextureID(const LoadedImage &image, Sampler sampler, TextureDetails textureDetails);
GLuint generateCubemapTextureID(const std::vector<LoadedImage> &imageData, TextureDetails textureDetails);
} // namespace textureUtilities

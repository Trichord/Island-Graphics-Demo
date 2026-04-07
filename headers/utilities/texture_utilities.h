#pragma once

#include <cstdint>
#include <vector>

#include "glad/glad.h"
#include "image_loader.h"
#include "textures.h"

// Request sRGB, request mipmaps, degree of anisotropy
struct TextureRequest
{
    bool sRGB{false};
    bool mipmaps{false};
    float anisotropic{0.0f};

    constexpr TextureRequest() = default;
    constexpr TextureRequest(bool requestSRGB, bool requestMipmaps, float ansiotropicDegree)
        : sRGB{requestSRGB}, mipmaps{requestMipmaps}, anisotropic{ansiotropicDegree}
    {
    }
};

namespace textureUtilities
{
Texture generateTexture(const LoadedImage &image, Sampler sampler, TextureRequest textureDetails);
Texture generateCubemapTexture(const std::vector<LoadedImage> &imageData, TextureRequest textureDetails);
} // namespace textureUtilities

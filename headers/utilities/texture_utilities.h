#pragma once

#include <vector>

#include "image_loader.h"
#include "textures.h"

// Request sRGB, request mipmaps, degree of anisotropy
struct TextureDetails
{
    bool sRGB{false};
    bool mipmaps{false};
    float anisotropic{0.0f};
};

namespace textureUtilities
{
int calculateMipmapLevels(int width, int height);
Texture generateTexture(const LoadedImage &image, Sampler sampler, TextureDetails details);
Texture generateCubemapTexture(const std::vector<LoadedImage> &imageData, TextureDetails details);
} // namespace textureUtilities

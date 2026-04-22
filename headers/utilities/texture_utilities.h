#pragma once

#include <vector>

#include "image_loader.h"
#include "textures.h"

// Request sRGB, request mipmaps, degree of anisotropy
struct TextureRequest
{
    bool sRGB{false};
    bool mipmaps{false};
    float anisotropic{0.0f};
};

namespace textureUtilities
{
int calculateMipmapLevels(int width, int height);
Texture generateTexture(const LoadedImage &image, Sampler sampler, TextureRequest textureDetails);
Texture generateCubemapTexture(const std::vector<LoadedImage> &imageData, TextureRequest textureDetails);
} // namespace textureUtilities

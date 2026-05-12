#pragma once

#include "glad/glad.h"
#include "texture_loader.h"
#include "textures.h"

struct TextureDetails
{
    Sampler sampler;
    bool sRGB{false};
    float anisotropicFiltering{0.0f};
};

namespace TextureUtilities
{
// Conversions
GLenum toSRGB(GLenum format);
GLenum gliToGL(const gli::texture &texture);

// Math
int calculateMipmapLevels(int width, int height);

// Texture creation
GLuint textureFromFile(std::filesystem::path path, TextureDetails details);
} // namespace TextureUtilities

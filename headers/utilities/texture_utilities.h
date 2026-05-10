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

namespace textureUtilities
{
GLenum toSRGB(GLenum format);
GLenum gliToGL(const gli::texture &texture);
int calculateMipmapLevels(int width, int height);
GLuint textureFromFile(std::filesystem::path path, TextureDetails details);
void insertIntoSamplerArray(const gli::texture &texture, GLuint arrayID, int arrayLayer, GLenum internalFormat);
} // namespace textureUtilities

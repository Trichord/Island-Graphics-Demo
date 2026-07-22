#pragma once

#include <filesystem>
#include <glad/glad.h>

#include "renderer/textures.h"
#include "utilities/texture_loader.h"
#include "utilities/ktx_texture.h"

struct TextureDetails
{
    Sampler sampler;
    bool sRGB{false};
    float anisotropicFiltering{0.0f};
};

namespace TextureUtilities
{
bool isCompressed(GLenum internalFormat);
bool transcodeKTX(KTXTexture &texture);
int calculateMipmapLevels(int width, int height, bool compressed);
GLenum vkFormatToGL(const ktx_uint32_t vkFormat);
GLuint textureFromFile(std::filesystem::path path, TextureDetails details);
GLuint createTextureID(int width, int height, GLenum internalFormat, Sampler sampler, bool mipmaps, float anisoLevel);
} // namespace TextureUtilities

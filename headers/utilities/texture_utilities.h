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
int calculateMipmapLevels(int width, int height);
gli::gl::internal_format toSRGB(gli::gl::internal_format format);
GLuint textureFromFile(std::filesystem::path path, TextureDetails details);
void insertIntoSamplerArray(GLuint arrayID, gli::texture &texture, int arrayLayer,
                            gli::gl::internal_format internalFormat);
} // namespace textureUtilities

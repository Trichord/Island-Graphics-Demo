#pragma once

#include <cstdint>
#include <vector>
#include <array>

#include "glad/glad.h"
#include "image_loader.h"

struct Sampler
{
    std::uint16_t magFilter, minFilter, wrapS, wrapT;

    Sampler(uint16_t mag = GL_LINEAR, uint16_t min = GL_LINEAR_MIPMAP_LINEAR, uint16_t ws = GL_REPEAT,
            uint16_t wt = GL_REPEAT)
        : magFilter(mag), minFilter(min), wrapS(ws), wrapT(wt) {};
};

struct TextureDetails
{
    bool sRGBRequested{false};
    bool anisoRequested{false};
    float anisoLevel{0.0f};
};

namespace textureUtilities
{
GLuint generateTextureID(const LoadedImage &image, Sampler sampler, TextureDetails config, bool globalAnsioEnabled);
GLuint generateCubemapTextureID(const std::vector<LoadedImage> &imageData, bool mipMaps, bool sRGB = true);
void generateCubemapFace(GLuint cubeMap, GLuint texture, int size, int faceIndex);
} // namespace textureUtilities

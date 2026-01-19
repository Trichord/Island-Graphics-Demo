#pragma once

#include <cstdint>
#include <vector>

#include "glad/glad.h"
#include "image_loader.h"

struct Sampler {
  std::uint16_t magFilter, minFilter, wrapS, wrapT;

  Sampler(uint16_t mag = GL_LINEAR, uint16_t min = GL_LINEAR_MIPMAP_LINEAR,
          uint16_t ws = GL_REPEAT, uint16_t wt = GL_REPEAT)
      : magFilter(mag), minFilter(min), wrapS(ws), wrapT(wt) {};
};

struct TextureConfig {
  bool sRGBRequested{false};
  bool anisoRequested{false};
  float anisoLevel{0.0f};
};

namespace textureUtilities {
GLuint generateTextureID(const LoadedImage& image, Sampler sampler,
                         TextureConfig config, bool globalAnsioEnabled);
GLuint generateCubemapTextureID(const std::vector<LoadedImage>& imageData,
                                bool sRGB = true);
}  // namespace textureUtilities

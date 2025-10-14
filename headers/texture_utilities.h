#pragma once

#include <glad/glad.h>

#include "image_loader.h"

struct Sampler {
  std::uint16_t magFilter, minFilter, wrapS, wrapT;

  Sampler(uint16_t mag = GL_LINEAR, uint16_t min = GL_LINEAR_MIPMAP_LINEAR,
          uint16_t ws = GL_REPEAT, uint16_t wt = GL_REPEAT)
      : magFilter(mag), minFilter(min), wrapS(ws), wrapT(wt) {};
};

namespace texture_utilities {
GLuint generateTextureID(const ImageData& image, bool sRGB);
GLuint generateTextureID(const ImageData& image, Sampler sampler,
                         unsigned int index, bool sRGB = false,
                         bool anisotropic = false);
GLuint generateCubemapTextureID(const std::vector<ImageData>& imageData,
                                bool sRGB = false, bool anisotropic = false);
GLuint generateQuadTexID(const ImageData& image, bool sRGB, bool anisotropic);
}  // namespace texture_utilities

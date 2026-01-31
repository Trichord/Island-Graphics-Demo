#include "render_target.h"

#include <algorithm>
#include <cmath>
#include <iostream>

RenderTarget::RenderTarget(int screenWidth, int screenHeight, float scale,
                           bool depthMapTexture)
    : width{static_cast<int>(screenWidth * scale)},
      height{static_cast<int>(screenHeight * scale)} {
  // Mipmaps
  int maxDimension = std::max(width, height);
  int mipLevels = static_cast<int>(std::floor(std::log2(maxDimension)) + 1);

  glCreateFramebuffers(1, &FBO);

  // Color texture
  glCreateTextures(GL_TEXTURE_2D, 1, &colorTextureID);
  glTextureStorage2D(colorTextureID, mipLevels, GL_SRGB8_ALPHA8, width, height);
  glTextureParameteri(colorTextureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTextureParameteri(colorTextureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTextureParameteri(colorTextureID, GL_TEXTURE_MIN_FILTER,
                      GL_LINEAR_MIPMAP_LINEAR);
  glTextureParameteri(colorTextureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Attach
  glNamedFramebufferTexture(FBO, GL_COLOR_ATTACHMENT0, colorTextureID, 0);

  if (depthMapTexture) {
    glCreateTextures(GL_TEXTURE_2D, 1, &depthMapTextureID);
    glTextureStorage2D(depthMapTextureID, 1, GL_DEPTH_COMPONENT24, width,
                       height);
    glTextureParameteri(depthMapTextureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(depthMapTextureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(depthMapTextureID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(depthMapTextureID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Attach
    glNamedFramebufferTexture(FBO, GL_DEPTH_ATTACHMENT, depthMapTextureID, 0);
  }

  if (glCheckNamedFramebufferStatus(FBO, GL_FRAMEBUFFER) !=
      GL_FRAMEBUFFER_COMPLETE)
    std::cerr << "ERROR: failed to create framebuffer" << std::endl;
}

RenderTarget::RenderTarget(RenderTarget&& other) noexcept
    : width{other.width},
      height{other.height},
      FBO{other.FBO},
      colorTextureID{other.colorTextureID},
      depthMapTextureID{other.depthMapTextureID} {
  other.FBO = 0;
  other.colorTextureID = 0;
  other.depthMapTextureID = 0;
}

RenderTarget& RenderTarget::operator=(RenderTarget&& other) noexcept {
  if (this != &other) {
    deleteResources();

    width = other.width;
    height = other.height;
    FBO = other.FBO;
    colorTextureID = other.colorTextureID;
    depthMapTextureID = other.depthMapTextureID;

    other.FBO = 0;
    other.colorTextureID = 0;
    other.depthMapTextureID = 0;
  }

  return *this;
}

RenderTarget::~RenderTarget() { deleteResources(); }

void RenderTarget::bind() { glBindFramebuffer(GL_FRAMEBUFFER, FBO); }

void RenderTarget::unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

GLuint RenderTarget::getDepthMapTextureID() { return depthMapTextureID; }

void RenderTarget::generateMipMaps() {
  glGenerateTextureMipmap(colorTextureID);
}

GLuint RenderTarget::getColorTextureID() { return colorTextureID; }

void RenderTarget::clear(bool color, bool depth) {
  if (color) glClearNamedFramebufferfv(FBO, GL_COLOR, 0, clearColor.data());
  if (depth) glClearNamedFramebufferfv(FBO, GL_DEPTH, 0, &depthColor);
}

void RenderTarget::deleteResources() {
  glDeleteTextures(1, &colorTextureID);
  if (depthMapTextureID) glDeleteTextures(1, &depthMapTextureID);
  glDeleteFramebuffers(1, &FBO);
}
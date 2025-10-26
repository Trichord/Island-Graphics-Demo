#include "texture_utilities.h"

#include <glad/glad.h>
#include <iostream>
#include "config_window.h"

GLuint textureUtilities::generateTextureID(const ImageData& image,
                                           Sampler sampler, bool sRGB,
                                           bool anisotropic) {
  GLuint textureID{0};

  if (image.data) {
    GLenum imageFormat{GL_RGB};
    GLenum internalFormat{GL_RGB};
    int nrChannels{image.nrChannels};

    // stb_image only ever returns a 1, 2, 3, or 4 for a valid image
    if (nrChannels == 1) {
      imageFormat = GL_RED;
      internalFormat = GL_RED;
    } else if (nrChannels == 2) {
      imageFormat = GL_RG;
      internalFormat = GL_RG;
    } else if (nrChannels == 3) {
      imageFormat = GL_RGB;
      internalFormat = sRGB ? GL_SRGB : GL_RGB;
    } else if (nrChannels == 4) {
      imageFormat = GL_RGBA;
      internalFormat = sRGB ? GL_SRGB_ALPHA : GL_RGBA;
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, image.width, image.height, 0,
                 imageFormat, GL_UNSIGNED_BYTE, image.data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampler.wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampler.wrapT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampler.minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampler.magFilter);

    if (anisotropic)
      glTexParameterf(
          GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY,
          ConfigWindow::globalGraphicsSettings.anisotropicFiltering);
  }

  glActiveTexture(GL_TEXTURE0);

  return textureID;
}

GLuint textureUtilities::generateCubemapTextureID(
    const std::vector<ImageData>& imageData, bool sRGB, bool anisotropic) {
  GLuint cubemapTextureID{0};
  glGenTextures(1, &cubemapTextureID);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTextureID);

  // Cubemap ID
  for (unsigned int i = 0; i < 6; ++i) {
    int nrChannels{imageData[i].nrChannels};

    if (!imageData[i].data) {
      std::cerr << "Failed to load cubemap texture" << std::endl;
      glDeleteTextures(1, &cubemapTextureID);
      return 0;
    }

    GLenum imageFormat{GL_RGB};
    GLenum internalFormat{GL_RGB};

    // stb_image only ever returns a 1, 2, 3, or 4 for a valid image
    if (nrChannels == 1) {
      imageFormat = GL_RED;
      internalFormat = GL_RED;
    } else if (nrChannels == 2) {
      imageFormat = GL_RG;
      internalFormat = GL_RG;
    } else if (nrChannels == 3) {
      imageFormat = GL_RGB;
      internalFormat = sRGB ? GL_SRGB : GL_RGB;
    } else if (nrChannels == 4) {
      imageFormat = GL_RGBA;
      internalFormat = sRGB ? GL_SRGB_ALPHA : GL_RGBA;
    }

    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat,
                 imageData[i].width, imageData[i].height, 0, imageFormat,
                 GL_UNSIGNED_BYTE, imageData[i].data);
  }

  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  if (anisotropic)
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY,
                    ConfigWindow::globalGraphicsSettings.anisotropicFiltering);

  glActiveTexture(GL_TEXTURE0);

  return cubemapTextureID;
}
#include "texture_utilities.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include "glad/glad.h"
#include "image_loader.h"

GLuint textureUtilities::generateTextureID(const LoadedImage &image, Sampler sampler, TextureDetails textureDetails)
{
    GLuint textureID{0};

    if (image.data)
    {
        GLenum imageFormat{GL_RGB};
        GLenum internalFormat{GL_RGB};
        int nrChannels{image.nrChannels};

        // stb_image only ever returns a 1, 2, 3, or 4 for a valid image
        if (nrChannels == 1)
        {
            imageFormat = GL_RED;
            internalFormat = GL_R8;
        }
        else if (nrChannels == 2)
        {
            imageFormat = GL_RG;
            internalFormat = GL_RG8;
        }
        else if (nrChannels == 3)
        {
            imageFormat = GL_RGB;
            internalFormat = textureDetails.sRGBRequested ? GL_SRGB8 : GL_RGB8;
        }
        else if (nrChannels == 4)
        {
            imageFormat = GL_RGBA;
            internalFormat = textureDetails.sRGBRequested ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        }

        // Max mipmap
        int maxDimension = std::max(image.width, image.height);
        int mipLevels = static_cast<int>(std::floor(std::log2(maxDimension)) + 1);

        // Create texture
        glCreateTextures(GL_TEXTURE_2D, 1, &textureID);
        glTextureStorage2D(textureID, mipLevels, internalFormat, image.width, image.height);
        glTextureSubImage2D(textureID, 0, 0, 0, image.width, image.height, imageFormat, GL_UNSIGNED_BYTE, image.data);

        glGenerateTextureMipmap(textureID);

        // Texture sampler options
        glTextureParameteri(textureID, GL_TEXTURE_WRAP_S, sampler.wrapS);
        glTextureParameteri(textureID, GL_TEXTURE_WRAP_T, sampler.wrapT);
        glTextureParameteri(textureID, GL_TEXTURE_MIN_FILTER, sampler.minFilter);
        glTextureParameteri(textureID, GL_TEXTURE_MAG_FILTER, sampler.magFilter);

        if (textureDetails.anisotropic > 0.0f)
            glTextureParameterf(textureID, GL_TEXTURE_MAX_ANISOTROPY, textureDetails.anisotropic);
    }

    return textureID;
}

GLuint textureUtilities::generateCubemapTextureID(const std::vector<LoadedImage> &imageData,
                                                  TextureDetails textureDetails)
{
    if (!imageData[0].data || imageData[0].width != imageData[0].height)
    {
        std::cerr << "Failed to load cubemap texture" << std::endl;
        return 0;
    }

    int nrChannels{imageData[0].nrChannels};
    GLuint cubemapTextureID{0};
    GLenum imageFormat{GL_RGB};
    GLenum internalFormat{GL_RGB};

    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &cubemapTextureID);

    // stb_image only ever returns a 1, 2, 3, or 4 for a valid image
    if (nrChannels == 1)
    {
        imageFormat = GL_RED;
        internalFormat = GL_R8;
    }
    else if (nrChannels == 2)
    {
        imageFormat = GL_RG;
        internalFormat = GL_RG8;
    }
    else if (nrChannels == 3)
    {
        imageFormat = GL_RGB;
        internalFormat = textureDetails.sRGBRequested ? GL_SRGB8 : GL_RGB8;
    }
    else if (nrChannels == 4)
    {
        imageFormat = GL_RGBA;
        internalFormat = textureDetails.sRGBRequested ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    }

    // Max mipmap, assuming first image is same resolution as the rest
    int maxDimension = std::max(imageData[0].width, imageData[0].height);
    int maxMipLevels = static_cast<int>(std::floor(std::log2(maxDimension)) + 1);

    int mipLevels = textureDetails.mipMapsRequested ? maxMipLevels : 1;

    glTextureStorage2D(cubemapTextureID, mipLevels, internalFormat, imageData[0].width, imageData[0].height);

    for (GLuint i = 0; i < 6; ++i)
        if (imageData[i].data)
            glTextureSubImage3D(cubemapTextureID, 0, 0, 0, i, imageData[i].width, imageData[i].height, 1, imageFormat,
                                GL_UNSIGNED_BYTE, imageData[i].data);

        else
        {
            std::cerr << "Failed to load cubemap texture" << std::endl;
            glDeleteTextures(1, &cubemapTextureID);
            return 0;
        }

    if (textureDetails.mipMapsRequested)
    {
        glGenerateTextureMipmap(cubemapTextureID);
        glTextureParameteri(cubemapTextureID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    else
        glTextureParameteri(cubemapTextureID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTextureParameteri(cubemapTextureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(cubemapTextureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(cubemapTextureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(cubemapTextureID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return cubemapTextureID;
}
#include "texture_utilities.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include "glad/glad.h"
#include "image_loader.h"
#include "textures.h"

int textureUtilities::calculateMipmapLevels(int width, int height)
{
    int maxDimension = std::max(width, height);
    return static_cast<int>(std::floor(std::log2(maxDimension)) + 1);
}

Texture magentaTexture()
{
    GLuint magenta{0};
    unsigned char color[] = {255, 0, 255};
    glCreateTextures(GL_TEXTURE_2D, 1, &magenta);
    glTextureStorage2D(magenta, 1, GL_SRGB8, 1, 1);
    glTextureSubImage2D(magenta, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, color);

    return {.id = magenta, .width = 1, .height = 1, .internalFormat = GL_SRGB8};
}

Texture textureUtilities::generateTexture(const LoadedImage &image, Sampler sampler, TextureDetails details)
{
    // Early return with debug magenta
    if (!image.data)
        return magentaTexture();

    GLuint textureID{0};
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
        internalFormat = details.sRGB ? GL_SRGB8 : GL_RGB8;
    }
    else if (nrChannels == 4)
    {
        imageFormat = GL_RGBA;
        internalFormat = details.sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    }

    int mipLevels = calculateMipmapLevels(image.width, image.height);

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

    if (details.anisotropic > 0.0f)
        glTextureParameterf(textureID, GL_TEXTURE_MAX_ANISOTROPY, details.anisotropic);

    return {.id = textureID, .width = image.width, .height = image.height, .internalFormat = internalFormat};
}

Texture magentaCubemap()
{
    GLuint magenta{0};
    unsigned char color[] = {255, 0, 255};
    glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &magenta);
    glTextureStorage2D(magenta, 1, GL_SRGB8, 1, 1);

    for (GLuint i = 0; i < 6; ++i)
        glTextureSubImage3D(magenta, 0, 0, 0, i, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, color);

    return {.id = magenta, .width = 1, .height = 1, .internalFormat = GL_SRGB8};
}

Texture textureUtilities::generateCubemapTexture(const std::vector<LoadedImage> &imageData, TextureDetails details)
{
    // Early return with debug magenta
    if (!imageData[0].data || imageData[0].width != imageData[0].height)
        return magentaCubemap();

    int nrChannels{imageData[0].nrChannels};

    for (size_t i = 0; i < imageData.size(); ++i)
    {
        if (imageData[i].nrChannels != nrChannels)
        {
            std::cerr << "ERROR: Mismatched number of cubemap channels" << std::endl;
            return magentaCubemap();
        }
    }

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
        internalFormat = details.sRGB ? GL_SRGB8 : GL_RGB8;
    }
    else if (nrChannels == 4)
    {
        imageFormat = GL_RGBA;
        internalFormat = details.sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
    }

    // Max mipmap, assuming first image is same resolution as the rest
    int maxMipLevels{calculateMipmapLevels(imageData[0].width, imageData[0].height)};
    int mipLevels{details.mipmaps ? maxMipLevels : 1};

    glTextureStorage2D(cubemapTextureID, mipLevels, internalFormat, imageData[0].width, imageData[0].height);

    for (GLuint i = 0; i < 6; ++i)
    {
        if (imageData[i].data)
            glTextureSubImage3D(cubemapTextureID, 0, 0, 0, i, imageData[i].width, imageData[i].height, 1, imageFormat,
                                GL_UNSIGNED_BYTE, imageData[i].data);

        else
        {
            std::cerr << "Failed to load cubemap texture" << std::endl;
            glDeleteTextures(1, &cubemapTextureID);
            return magentaCubemap();
        }
    }

    if (details.mipmaps)
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

    return {.id = cubemapTextureID,
            .width = imageData[0].width,
            .height = imageData[0].height,
            .internalFormat = internalFormat};
}
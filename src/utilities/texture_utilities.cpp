#include "texture_utilities.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <vector>

#include "glad/glad.h"
#include "texture_loader.h"
#include "textures.h"

namespace
{

GLuint magentaTexture()
{
    GLuint magenta{0};
    unsigned char color[] = {255, 0, 255};
    glCreateTextures(GL_TEXTURE_2D, 1, &magenta);
    glTextureStorage2D(magenta, 1, GL_SRGB8, 1, 1);
    glTextureSubImage2D(magenta, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, color);

    glTextureParameteri(magenta, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(magenta, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureParameteri(magenta, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(magenta, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return magenta;
}
} // namespace

gli::gl::internal_format textureUtilities::toSRGB(gli::gl::internal_format format)
{
    using enum gli::gl::internal_format;

    switch (format)
    {
    // Uncompressed
    case INTERNAL_R8_UNORM:
        return INTERNAL_SR8;
    case INTERNAL_RG8_UNORM:
        return INTERNAL_SRG8;
    case INTERNAL_RGB8_UNORM:
        return INTERNAL_SRGB8;
    case INTERNAL_RGBA8_UNORM:
        return INTERNAL_SRGB8_ALPHA8;

    // DXT/BC
    case INTERNAL_RGB_DXT1:
        return INTERNAL_SRGB_DXT1;
    case INTERNAL_RGBA_DXT1:
        return INTERNAL_SRGB_ALPHA_DXT1;
    case INTERNAL_RGBA_DXT3:
        return INTERNAL_SRGB_ALPHA_DXT3;
    case INTERNAL_RGBA_DXT5:
        return INTERNAL_SRGB_ALPHA_DXT5;

    // BPTC/BC7
    case INTERNAL_RGB_BP_UNORM:
        return INTERNAL_SRGB_BP_UNORM;

    // PVRTC
    case INTERNAL_RGB_PVRTC_2BPPV1:
        return INTERNAL_SRGB_PVRTC_2BPPV1;
    case INTERNAL_RGB_PVRTC_4BPPV1:
        return INTERNAL_SRGB_PVRTC_4BPPV1;
    case INTERNAL_RGBA_PVRTC_2BPPV1:
        return INTERNAL_SRGB_ALPHA_PVRTC_2BPPV1;
    case INTERNAL_RGBA_PVRTC_4BPPV1:
        return INTERNAL_SRGB_ALPHA_PVRTC_4BPPV1;
    case INTERNAL_RGBA_PVRTC_2BPPV2:
        return INTERNAL_SRGB_ALPHA_PVRTC_2BPPV2;
    case INTERNAL_RGBA_PVRTC_4BPPV2:
        return INTERNAL_SRGB_ALPHA_PVRTC_4BPPV2;

    // ETC
    case INTERNAL_RGB_ETC2:
        return INTERNAL_SRGB8_ETC2;
    case INTERNAL_RGBA_PUNCHTHROUGH_ETC2:
        return INTERNAL_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2;
    case INTERNAL_RGBA_ETC2:
        return INTERNAL_SRGB8_ALPHA8_ETC2_EAC;

    // ASTC
    case INTERNAL_RGBA_ASTC_4x4:
        return INTERNAL_SRGB8_ALPHA8_ASTC_4x4;
    case INTERNAL_RGBA_ASTC_5x4:
        return INTERNAL_SRGB8_ALPHA8_ASTC_5x4;
    case INTERNAL_RGBA_ASTC_5x5:
        return INTERNAL_SRGB8_ALPHA8_ASTC_5x5;
    case INTERNAL_RGBA_ASTC_6x5:
        return INTERNAL_SRGB8_ALPHA8_ASTC_6x5;
    case INTERNAL_RGBA_ASTC_6x6:
        return INTERNAL_SRGB8_ALPHA8_ASTC_6x6;
    case INTERNAL_RGBA_ASTC_8x5:
        return INTERNAL_SRGB8_ALPHA8_ASTC_8x5;
    case INTERNAL_RGBA_ASTC_8x6:
        return INTERNAL_SRGB8_ALPHA8_ASTC_8x6;
    case INTERNAL_RGBA_ASTC_8x8:
        return INTERNAL_SRGB8_ALPHA8_ASTC_8x8;
    case INTERNAL_RGBA_ASTC_10x5:
        return INTERNAL_SRGB8_ALPHA8_ASTC_10x5;
    case INTERNAL_RGBA_ASTC_10x6:
        return INTERNAL_SRGB8_ALPHA8_ASTC_10x6;
    case INTERNAL_RGBA_ASTC_10x8:
        return INTERNAL_SRGB8_ALPHA8_ASTC_10x8;
    case INTERNAL_RGBA_ASTC_10x10:
        return INTERNAL_SRGB8_ALPHA8_ASTC_10x10;
    case INTERNAL_RGBA_ASTC_12x10:
        return INTERNAL_SRGB8_ALPHA8_ASTC_12x10;
    case INTERNAL_RGBA_ASTC_12x12:
        return INTERNAL_SRGB8_ALPHA8_ASTC_12x12;

    // Already sRGB or no sRGB equivalent exists
    default:
        return format;
    }
}

int textureUtilities::calculateMipmapLevels(int width, int height)
{
    int maxDimension = std::max(width, height);
    return static_cast<int>(std::floor(std::log2(maxDimension)) + 1);
}

void textureUtilities::insertIntoSamplerArray(GLuint arrayID, gli::texture &texture, int arrayLayer,
                                              gli::gl::internal_format internalFormat)
{
    GLenum format{static_cast<GLenum>(internalFormat)};

    for (std::size_t layer{0}; layer < texture.layers(); ++layer)
        for (std::size_t face{0}; face < texture.faces(); ++face)
            for (std::size_t level{0}; level < texture.levels(); ++level)
            {
                std::size_t zOffset{static_cast<std::size_t>(arrayLayer) + layer * texture.faces() + face};
                glm::tvec3<GLsizei> extent{texture.extent(level)};

                glCompressedTextureSubImage3D(arrayID, static_cast<GLint>(level), 0, 0, static_cast<GLint>(zOffset),
                                              extent.x, extent.y, 1, format, static_cast<GLsizei>(texture.size(level)),
                                              texture.data(layer, face, level));
            }
}

GLuint textureUtilities::textureFromFile(std::filesystem::path path, TextureDetails details)
{
    gli::texture texture{gli::load(path.string().c_str())};

    if (texture.empty())
    {
        std::cerr << "ERROR: Texture loading error for " << path << std::endl;
        return magentaTexture();
    }

    const gli::gl glTranslator{gli::gl::PROFILE_GL33};
    const gli::gl::format glFormat{glTranslator.translate(texture.format(), texture.swizzles())};
    const GLenum target{static_cast<GLenum>(glTranslator.translate(texture.target()))};
    const gli::texture::target_type targetType{texture.target()};

    if (targetType != gli::TARGET_1D_ARRAY && targetType != gli::TARGET_2D)
    {
        std::cerr << "ERROR: Wrong texture type for 2D texture for " << path << std::endl;
        return magentaTexture();
    }

    GLuint id{0};
    glCreateTextures(target, 1, &id);

    glm::tvec3<GLsizei> maxWidth{texture.extent()};
    gli::gl::internal_format internalFormat{glFormat.Internal};

    if (details.sRGB)
        internalFormat = toSRGB(internalFormat);

    glTextureStorage2D(id, static_cast<GLint>(texture.levels()), static_cast<GLenum>(internalFormat), maxWidth.x,
                       maxWidth.y);

    for (std::size_t layer{0}; layer < texture.layers(); ++layer)
        for (std::size_t face{0}; face < texture.faces(); ++face)
            for (std::size_t level{0}; level < texture.levels(); ++level)
            {
                const glm::tvec3<GLsizei> extent{texture.extent(level)};

                if (gli::is_compressed(texture.format()))
                {

                    glCompressedTextureSubImage2D(id, static_cast<GLint>(level), 0, 0, extent.x, extent.y,
                                                  glFormat.Internal, static_cast<GLsizei>(texture.size(level)),
                                                  texture.data(layer, face, level));
                }
                else
                {
                    glTextureSubImage2D(id, static_cast<GLint>(level), 0, 0, extent.x, extent.y, glFormat.External,
                                        glFormat.Type, texture.data(layer, face, level));
                }
            }

    glTextureParameteri(id, GL_TEXTURE_WRAP_S, details.sampler.wrapS);
    glTextureParameteri(id, GL_TEXTURE_WRAP_T, details.sampler.wrapT);
    glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, details.sampler.minFilter);
    glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, details.sampler.magFilter);

    if (details.anisotropicFiltering > 0.0f)
        glTextureParameterf(id, GL_TEXTURE_MAX_ANISOTROPY, details.anisotropicFiltering);

    return id;
}
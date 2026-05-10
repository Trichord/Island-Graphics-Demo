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

GLenum textureUtilities::gliToGL(const gli::texture &texture)
{
    static const gli::gl glTranslator(gli::gl::PROFILE_GL33);

    const gli::gl::format format = glTranslator.translate(texture.format(), texture.swizzles());
    const glm::tvec3<GLsizei> extent{texture.extent()};
    const int size{static_cast<int>(extent.x)};

    return static_cast<GLenum>(format.Internal);
}

GLenum textureUtilities::toSRGB(GLenum format)
{
    switch (format)
    {
    // Uncompressed
    case GL_RGB8:
        return GL_SRGB8;
    case GL_RGBA8:
        return GL_SRGB8_ALPHA8;

    // BPTC/BC7
    case GL_COMPRESSED_RGBA_BPTC_UNORM:
        return GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;

    // ETC2
    case GL_COMPRESSED_RGB8_ETC2:
        return GL_COMPRESSED_SRGB8_ETC2;
    case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
        return GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2;
    case GL_COMPRESSED_RGBA8_ETC2_EAC:
        return GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC;

    // Already sRGB or no sRGB equivalent exists
    default:
        return format;
    }
}

int textureUtilities::calculateMipmapLevels(int width, int height)
{
    int maxDimension = std::max(width, height);

    // Only return down to a 4x4 for block compressed textures
    return static_cast<int>(std::floor(std::log2(maxDimension / 4))) + 1;
}

void textureUtilities::insertIntoSamplerArray(const gli::texture &texture, GLuint arrayID, int arrayLayer,
                                              GLenum internalFormat)
{
    for (std::size_t layer{0}; layer < texture.layers(); ++layer)
        for (std::size_t face{0}; face < texture.faces(); ++face)
            for (std::size_t level{0}; level < texture.levels(); ++level)
            {
                std::size_t zOffset{static_cast<std::size_t>(arrayLayer) + layer * texture.faces() + face};
                glm::tvec3<GLsizei> extent{texture.extent(level)};

                if (extent.x < 4 || extent.y < 4)
                    break;

                glCompressedTextureSubImage3D(
                    arrayID, static_cast<GLint>(level), 0, 0, static_cast<GLint>(zOffset), extent.x, extent.y, 1,
                    internalFormat, static_cast<GLsizei>(texture.size(level)), texture.data(layer, face, level));
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
    GLenum internalFormat{static_cast<GLenum>(glFormat.Internal)};

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
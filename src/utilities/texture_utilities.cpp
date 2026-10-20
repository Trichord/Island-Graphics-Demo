#include "utilities/texture_utilities.h"

#include <algorithm>
#include <filesystem>
#include <glad/glad.h>
#include <iostream>

#include "renderer/textures.h"
#include "utilities/ktx_texture.h"
#include "utilities/vkformat_enum.h"

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

bool texture::isCompressed(GLenum internalFormat)
{
    switch (internalFormat)
    {
    case GL_COMPRESSED_RED_RGTC1:
    case GL_COMPRESSED_SIGNED_RED_RGTC1:
    case GL_COMPRESSED_RG_RGTC2:
    case GL_COMPRESSED_SIGNED_RG_RGTC2:
    case GL_COMPRESSED_RGBA_BPTC_UNORM:
    case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:
        return true;

    default:
        return false;
    }
}

GLenum texture::vkFormatToGL(const ktx_uint32_t vkFormat)
{
    switch (vkFormat)
    {
    case VK_FORMAT_BC4_UNORM_BLOCK:
        return GL_COMPRESSED_RED_RGTC1;
    case VK_FORMAT_BC4_SNORM_BLOCK:
        return GL_COMPRESSED_SIGNED_RED_RGTC1;

    case VK_FORMAT_BC5_UNORM_BLOCK:
        return GL_COMPRESSED_RG_RGTC2;
    case VK_FORMAT_BC5_SNORM_BLOCK:
        return GL_COMPRESSED_SIGNED_RG_RGTC2;

    case VK_FORMAT_BC7_UNORM_BLOCK:
        return GL_COMPRESSED_RGBA_BPTC_UNORM;
    case VK_FORMAT_BC7_SRGB_BLOCK:
        return GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;

    default:
        std::cerr << "WARNING: Unsupported vkFormat " << vkFormat << "\n";
        return 0;
    }
}

int texture::calculateMipmapLevels(int width, int height, bool compressed)
{
    int mipCount{1};
    int maxDimension{std::max(width, height)};
    int maxDepth{compressed ? 4 : 1};

    while ((maxDimension >> mipCount) >= maxDepth)
        ++mipCount;

    return mipCount;
}

bool texture::transcodeKTX(KTXTexture &texture)
{
    if (!texture)
    {
        std::cerr << "Error: empty texture\n";
        return false;
    }

    if (!ktxTexture2_NeedsTranscoding(texture.get()))
        return true;

    const ktx_uint32_t numComponents{ktxTexture2_GetNumComponents(texture.get())};
    ktx_transcode_fmt_e format;

    switch (numComponents)
    {
    case 1:
        format = KTX_TTF_BC4_R;
        break;
    case 2:
        format = KTX_TTF_BC5_RG;
        break;
    default:
        format = KTX_TTF_BC7_RGBA;
        break;
    }

    KTX_error_code result{ktxTexture2_TranscodeBasis(texture.get(), format, 0)};

    if (result != KTX_SUCCESS)
    {
        std::cerr << ktxErrorString(result) << "\n";
        return false;
    }

    return true;
}

// Loads single 2D images from disk into Intended for User Interface, Logos, etc.
GLuint texture::textureFromFile(std::filesystem::path path, TextureDetails details)
{
    if (path.empty() || !std::filesystem::exists(path))
    {
        std::cerr << "Error: Bad filepath for texture " << path << "\n";
        return magentaTexture();
    }

    KTXTexture texture{path.string().c_str()};

    if (!transcodeKTX(texture))
    {
        std::cerr << "Failed to transcode " << path << "\n";
        return magentaTexture();
    }

    GLuint id{0};
    GLenum target{GL_TEXTURE_2D};
    GLenum error{GL_NONE};

    ktx_error_code_e result{ktxTexture_GLUpload(ktxTexture(texture.get()), &id, &target, &error)};

    if (result != KTX_SUCCESS)
    {
        std::cerr << "Error: Wrong texture type for 2D texture for " << path << "\n";
        glDeleteTextures(1, &id);
        return magentaTexture();
    }

    if (error != GL_NONE)
    {
        std::cerr << "Error: OpenGL error " << error << " creating texture from " << path << "\n";
        glDeleteTextures(1, &id);
        return magentaTexture();
    }

    if (target != GL_TEXTURE_2D && target != GL_IMAGE_1D_ARRAY)
    {
        std::cerr << "Error: Wrong texture type for 2D texture for " << path << "\n";
        glDeleteTextures(1, &id);
        return magentaTexture();
    }

    glTextureParameteri(id, GL_TEXTURE_WRAP_S, details.sampler.wrapS);
    glTextureParameteri(id, GL_TEXTURE_WRAP_T, details.sampler.wrapT);
    glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, details.sampler.minFilter);
    glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, details.sampler.magFilter);

    if (details.anisotropicFiltering > 0.0f)
        glTextureParameterf(id, GL_TEXTURE_MAX_ANISOTROPY, details.anisotropicFiltering);

    return id;
}

GLuint texture::createTextureID(int width, int height, GLenum internalFormat, Sampler sampler, bool mipmaps,
                                         float anisoLevel)
{
    GLuint id{0};
    int mipLevels{mipmaps ? texture::calculateMipmapLevels(width, height, false) : 1};

    glCreateTextures(GL_TEXTURE_2D, 1, &id);

    glTextureParameteri(id, GL_TEXTURE_WRAP_S, sampler.wrapS);
    glTextureParameteri(id, GL_TEXTURE_WRAP_T, sampler.wrapT);
    glTextureParameteri(id, GL_TEXTURE_MIN_FILTER, sampler.minFilter);
    glTextureParameteri(id, GL_TEXTURE_MAG_FILTER, sampler.magFilter);

    if (anisoLevel > 0.0f)
        glTextureParameterf(id, GL_TEXTURE_MAX_ANISOTROPY, anisoLevel);

    glTextureStorage2D(id, static_cast<GLsizei>(mipLevels), internalFormat, static_cast<GLsizei>(width),
                       static_cast<GLsizei>(height));

    return id;
}
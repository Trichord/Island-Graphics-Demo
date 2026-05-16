#include "texture_utilities.h"

#include "ktx_texture.h"

#include <algorithm>
#include <filesystem>
#include <iostream>

#include "KHR/vkformat_enum.h"
#include "glad/glad.h"
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

GLenum TextureUtilities::vkFormatToGL(const ktx_uint32_t vkFormat)
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
        std::cerr << "WARNING: Unsupported vkFormat " << vkFormat << std::endl;
        return 0;
    }
}

int TextureUtilities::calculateMipmapLevels(int width, int height, bool compressed)
{
    int mipCount{1};
    int minDimension{std::min(width, height)};
    int maxDepth{compressed ? 4 : 1};

    while ((minDimension >> mipCount) >= maxDepth)
        ++mipCount;

    return mipCount;
}

bool TextureUtilities::transcodeKTX(KTXTexture &texture)
{
    if (!texture)
    {
        std::cerr << "ERROR: empty texture" << std::endl;
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
        std::cerr << ktxErrorString(result) << std::endl;
        return false;
    }

    return true;
}

// Loads single 2D images from disk into Intended for User Interface, Logos, etc.
GLuint TextureUtilities::textureFromFile(std::filesystem::path path, TextureDetails details)
{
    if (path.empty() || !std::filesystem::exists(path))
    {
        std::cerr << "ERROR: Bad filepath for texture " << path << std::endl;
        return magentaTexture();
    }

    KTXTexture texture{path.string().c_str()};

    if (!transcodeKTX(texture))
    {
        std::cerr << "Failed to transcode " << path << std::endl;
        return magentaTexture();
    }

    GLuint id{0};
    GLenum target{GL_TEXTURE_2D};
    GLenum error{GL_NONE};

    ktx_error_code_e result{ktxTexture_GLUpload(ktxTexture(texture.get()), &id, &target, &error)};

    if (result != KTX_SUCCESS)
    {
        std::cerr << "ERROR: Wrong texture type for 2D texture for " << path << std::endl;
        glDeleteTextures(1, &id);
        return magentaTexture();
    }

    if (error != GL_NONE)
    {
        std::cerr << "ERROR: OpenGL error " << error << " creating texture from " << path << std::endl;
        glDeleteTextures(1, &id);
        return magentaTexture();
    }

    if (target != GL_TEXTURE_2D && target != GL_IMAGE_1D_ARRAY)
    {
        std::cerr << "ERROR: Wrong texture type for 2D texture for " << path << std::endl;
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
#include "array_texture.h"

#include <iostream>
#include <stdexcept>
#include <utility>

#include "glad/glad.h"
#include "glm/glm.hpp"
#include "ktx_texture.h"
#include "render_target.h"
#include "texture_utilities.h"

ArrayTexture::ArrayTexture(int width, int height, GLenum internalFormat, int capacity)
    : width_{width}, height_{height}, capacity_{capacity}, internalFormat_{internalFormat}
{
}

ArrayTexture::ArrayTexture(ArrayTexture &&other) noexcept
    : id_{other.id_}, width_{other.width_}, height_{other.height_}, current_{other.current_},
      capacity_{other.capacity_}, internalFormat_{other.internalFormat_}, cubemaps_{other.cubemaps_},
      allocated_{other.allocated_}
{
    other.id_ = 0;
}

ArrayTexture &ArrayTexture::operator=(ArrayTexture &&other) noexcept
{
    if (this != &other)
    {
        std::swap(id_, other.id_);
        std::swap(width_, other.width_);
        std::swap(height_, other.height_);
        std::swap(current_, other.current_);
        std::swap(capacity_, other.capacity_);
        std::swap(internalFormat_, other.internalFormat_);
        std::swap(cubemaps_, other.cubemaps_);
        std::swap(allocated_, other.allocated_);
    }

    return *this;
}

ArrayTexture::~ArrayTexture()
{
    if (id_)
        glDeleteTextures(1, &id_);
}

int ArrayTexture::getWidth() const
{
    return width_;
}

int ArrayTexture::getHeight() const
{
    return height_;
}

int ArrayTexture::getCapacity() const
{
    return capacity_;
}

int ArrayTexture::getCurrentSize() const
{
    return current_;
}

GLenum ArrayTexture::getInternalFormat() const
{
    return internalFormat_;
}

void ArrayTexture::addCapacity(int capacity)
{
    constexpr int maxArrayTextureLayers{2048};

    if (allocated_)
    {
        std::cerr << "ERROR: ArrayTexture::addCapacity called after ArrayTexture::allocate()" << std::endl;
        throw std::logic_error("ArrayTexture::addCapacity called after ArrayTexture::allocate()");
    }

    if (capacity_ + capacity > maxArrayTextureLayers)
    {
        std::cerr << "ERROR: ArrayTexture " << width_ << "x" << height_ << ", format " << internalFormat_
                  << " cannot allocate more than " << maxArrayTextureLayers << " layers" << std::endl;
        throw std::runtime_error("ArrayTexture over allocated");
    }

    capacity_ += capacity;
}

void ArrayTexture::bindTextureUnit(int unit)
{
    glBindTextureUnit(static_cast<GLuint>(unit), id_);
}

void ArrayTexture::generateMipmaps()
{
    if (!allocated_)
    {
        std::cerr << "ERROR: ArrayTexture::generateMipmaps called before ArrayTexture::allocate" << std::endl;
        throw std::logic_error("ArrayTexture::generateMipmaps called before ArrayTexture::allocate");
    }

    if (id_)
        glGenerateTextureMipmap(id_);
}

void ArrayTexture::allocate(bool cubemap, bool textureRepeat, bool mipmaps, float anisotropy)
{
    if (allocated_)
    {
        std::cerr << "ERROR: ArrayTexture::allocate() already called" << std::endl;
        throw std::logic_error("ArrayTexture::allocate() already called");
    }

    // Set allocated immediately to enforce the set order of function calls.
    allocated_ = true;

    if (!capacity_)
        return;

    cubemaps_ = cubemap;

    GLenum target{static_cast<GLenum>(cubemap ? GL_TEXTURE_CUBE_MAP_ARRAY : GL_TEXTURE_2D_ARRAY)};
    GLsizei depth{static_cast<GLsizei>(cubemap ? capacity_ * 6 : capacity_)};
    GLenum repeat{static_cast<GLenum>(textureRepeat ? GL_REPEAT : GL_CLAMP_TO_EDGE)};

    int mipLevels{mipmaps ? TextureUtilities::calculateMipmapLevels(width_, height_, true) : 1};

    glCreateTextures(target, 1, &id_);
    glTextureStorage3D(id_, static_cast<GLsizei>(mipLevels), internalFormat_, static_cast<GLsizei>(width_),
                       static_cast<GLsizei>(height_), depth);

    if (cubemap)
    {
        glTextureParameteri(id_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(id_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(id_, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }
    else
    {
        glTextureParameteri(id_, GL_TEXTURE_WRAP_S, repeat);
        glTextureParameteri(id_, GL_TEXTURE_WRAP_T, repeat);
    }

    glTextureParameteri(id_, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(id_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (anisotropy > 0.0f)
        glTextureParameterf(id_, GL_TEXTURE_MAX_ANISOTROPY, anisotropy);
}

int ArrayTexture::insert(RenderTarget &renderTarget)
{
    if (!allocated_)
    {
        std::cerr << "ERROR: ArrayTexture::insert called before ArrayTexture::allocate" << std::endl;
        throw std::logic_error("ArrayTexture::insert called before ArrayTexture::allocate");
    }

    const GLuint sourceID{renderTarget.getColorTextureID()};
    const GLuint layers{static_cast<GLuint>(renderTarget.getNumLayers())};
    const GLsizei sourceWidth{static_cast<GLsizei>(renderTarget.getWidth())};
    const GLsizei sourceHeight{static_cast<GLsizei>(renderTarget.getHeight())};

    if (sourceWidth > width_ || sourceHeight > height_)
    {
        std::cerr << "ERROR: Texture source " << sourceWidth << "x" << sourceHeight << " larger than ArrayTexture "
                  << width_ << "x" << height_ << ", format " << internalFormat_ << std::endl;

        throw std::runtime_error("Source texture too big for ArrayTexture");
    }

    if (current_ >= capacity_)
    {
        std::cerr << "ERROR: Array texture " << width_ << "x" << height_ << ", format " << internalFormat_
                  << " capacity reached" << std::endl;

        throw std::runtime_error("ArrayTexture overflow");
    }

    const GLenum target{static_cast<GLenum>(cubemaps_ ? GL_TEXTURE_CUBE_MAP_ARRAY : GL_TEXTURE_2D_ARRAY)};
    const GLenum sourceTarget{static_cast<GLenum>(layers > 1 ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D)};
    const GLint layer{static_cast<GLint>(cubemaps_ ? current_ * 6 : current_)};

    glCopyImageSubData(sourceID, sourceTarget, 0, 0, 0, 0, id_, target, 0, 0, 0, layer, sourceWidth, sourceHeight,
                       layers);

    return current_++;
}

int ArrayTexture::insert(const KTXTexture &texture)
{
    if (!allocated_)
    {
        std::cerr << "ERROR: ArrayTexture::insert called before ArrayTexture::allocate" << std::endl;
        throw std::logic_error("ArrayTexture::insert called before ArrayTexture::allocate");
    }

    if (!id_)
    {
        std::cerr << "ERROR: Uninitialized array texture " << width_ << "x" << height_ << ", format " << internalFormat_
                  << std::endl;

        throw std::runtime_error("Uninitialized array texture");
    }

    if (current_ >= capacity_)
    {
        std::cerr << "ERROR: Array texture " << width_ << "x" << height_ << ", format " << internalFormat_
                  << " capacity reached" << std::endl;

        throw std::runtime_error("ArrayTexture overflow");
    }

    validate(texture);
    insertIntoArray(texture, current_);
    return current_++;
}

void ArrayTexture::insertIntoArray(const KTXTexture &texture, int offset)
{
    for (ktx_uint32_t layer{0}; layer < texture->numLayers; ++layer)
        for (ktx_uint32_t face{0}; face < texture->numFaces; ++face)
            for (ktx_uint32_t level{0}; level < texture.maxLevels(); ++level)
            {
                KTXTexture::MipData mip{texture.getMipData(level, layer, face)};
                std::size_t zOffset{(static_cast<std::size_t>(offset) + layer) * texture->numFaces + face};

                glCompressedTextureSubImage3D(id_, static_cast<GLint>(level), 0, 0, static_cast<GLint>(zOffset),
                                              mip.width, mip.height, 1, internalFormat_, static_cast<GLsizei>(mip.size),
                                              mip.data);
            }
}

void ArrayTexture::validate(const KTXTexture &texture)
{
    bool valid{true};

    if (!texture)
    {
        std::cerr << "ERROR: Empty texture for ArrayTexture " << width_ << "x" << height_ << ", format "
                  << internalFormat_ << std::endl;
        throw std::runtime_error("Texture validation error");
    }

    int width{static_cast<int>(texture->baseWidth)};
    int height{static_cast<int>(texture->baseHeight)};

    if (width != width_ || height != height_)
    {
        std::cerr << "ERROR: Size mismatch: " << width << "x" << height;
        valid = false;
    }

    if (texture->isCubemap && !cubemaps_)
    {
        std::cerr << "ERROR: cubemap " << width << "x" << height;
        valid = false;
    }

    else if (!texture->isCubemap && cubemaps_)
    {
        std::cerr << "ERROR: 2D Texture " << width << "x" << height << " passed to cubemap";
        valid = false;
    }

    GLenum format{TextureUtilities::vkFormatToGL(texture->vkFormat)};

    if (format != internalFormat_)
    {
        std::cerr << "ERROR: Format " << format << " mismatch";
        valid = false;
    }

    if (!valid)
    {
        std::cerr << " ArrayTexture " << width_ << "x" << height_ << ", format " << internalFormat_ << std::endl;
        throw std::runtime_error("Texture validation error");
    }
}

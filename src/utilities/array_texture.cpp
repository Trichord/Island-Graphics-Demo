#include "array_texture.h"

#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string.h>
#include <utility>
#include <vector>

#include "glad/glad.h"
#include "glm/glm.hpp"
#include "ktx_texture.h"
#include "render_target.h"
#include "texture_utilities.h"

namespace
{
void capacityError(int width, int height, GLenum format, std::size_t attempted, std::size_t max)
{
    std::cerr << "ERROR: ArrayTexture " << width << "x" << height << ", format " << format << " cannot allocate "
              << attempted << ", max " << max << " layers" << std::endl;
    throw std::runtime_error("ArrayTexture over allocated");
}
} // namespace

ArrayTexture::ArrayTexture(int width, int height, GLenum internalFormat, bool fallback, std::size_t capacity)
    : width_{width}, height_{height}, capacity_{capacity}, internalFormat_{internalFormat}, fallback_{fallback}
{
    if (capacity > maxArrayTextureLayers_)
        capacityError(width, height, internalFormat, capacity, maxArrayTextureLayers_);

    if (fallback_ && capacity_ > maxArrayTextureLayers_ - 1)
        capacityError(width_, height_, internalFormat_, current_, maxArrayTextureLayers_);
    else if (fallback_)
        ++capacity_;
}

ArrayTexture::ArrayTexture(ArrayTexture &&other) noexcept
    : id_{other.id_}, width_{other.width_}, height_{other.height_}, current_{other.current_},
      capacity_{other.capacity_}, internalFormat_{other.internalFormat_}, cubemaps_{other.cubemaps_},
      allocated_{other.allocated_}, fallback_{other.fallback_}
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
        std::swap(fallback_, other.fallback_);
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

std::size_t ArrayTexture::getCapacity() const
{
    return capacity_;
}

std::size_t ArrayTexture::getCurrentSize() const
{
    return current_;
}

GLenum ArrayTexture::getInternalFormat() const
{
    return internalFormat_;
}

std::optional<int> ArrayTexture::getFallbackLayer() const
{
    if (fallback_)
        return 0;
    else
        return {};
}

void ArrayTexture::addCapacity(std::size_t capacity)
{
    if (allocated_)
    {
        std::cerr << "ERROR: ArrayTexture::addCapacity called after ArrayTexture::allocate()" << std::endl;
        throw std::logic_error("ArrayTexture::addCapacity called after ArrayTexture::allocate()");
    }

    if (capacity_ + capacity > maxArrayTextureLayers_)
        capacityError(width_, height_, internalFormat_, capacity, maxArrayTextureLayers_);

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

    if (fallback_)
    {
        createFallbackLayer();
        ++current_;
    }
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

    if (current_ > capacity_)
        capacityError(width_, height_, internalFormat_, current_, maxArrayTextureLayers_);

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

    if (current_ > capacity_)
        capacityError(width_, height_, internalFormat_, current_, maxArrayTextureLayers_);

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

void ArrayTexture::createFallbackLayer()
{
    if (internalFormat_ == GL_SRGB8)
        createSRGBFallback();
    else if (internalFormat_ == GL_COMPRESSED_RG_RGTC2)
        createRGRGTC2Fallback();
    else
    {
        std::cerr << "ERROR: Cannot create fallback texture for ArrayTexture " << width_ << "x" << height_
                  << ", format " << internalFormat_ << std::endl;
        throw std::runtime_error("Invalid fallback texture format");
    }
}

void ArrayTexture::createSRGBFallback()
{
    std::vector<std::uint8_t> pixels(width_ * height_ * 3, 0);
    for (std::size_t i{0}; i < pixels.size(); i += 3)
        pixels[i] = 255;

    int numFaces{cubemaps_ ? 6 : 1};

    for (int face{0}; face < numFaces; ++face)
    {
        glTextureSubImage3D(id_, 0, 0, 0, face, static_cast<GLsizei>(width_), static_cast<GLsizei>(height_), 1, GL_RGB,
                            GL_UNSIGNED_BYTE, pixels.data());
    }

    generateMipmaps();
}

void ArrayTexture::createRGRGTC2Fallback()
{
    // Solid red RGTC2
    constexpr std::uint8_t block[16] = {255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    int numFaces{cubemaps_ ? 6 : 1};
    int levels{TextureUtilities::calculateMipmapLevels(width_, height_, true)};

    for (int level{0}; level < levels; ++level)
    {
        int width{width_ >> level};
        int height{height_ >> level};

        std::vector<std::uint8_t> blocks(width / 4 * height / 4 * 16);

        for (std::size_t i{0}; i < blocks.size(); i += 16)
            std::memcpy(&blocks[i], block, 16);

        for (int face{0}; face < numFaces; ++face)
        {
            int zOffset{cubemaps_ ? face : 0};

            glCompressedTextureSubImage3D(id_, static_cast<GLint>(level), 0, 0, static_cast<GLint>(zOffset),
                                          static_cast<GLsizei>(width), static_cast<GLsizei>(height), 1,
                                          GL_COMPRESSED_RG_RGTC2, static_cast<GLsizei>(blocks.size()), blocks.data());
        }
    }
}

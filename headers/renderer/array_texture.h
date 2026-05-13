#pragma once

#include "glad/glad.h"
#include "gli/texture.hpp"
#include "render_target.h"

class ArrayTexture
{
  public:
    ArrayTexture() = default;
    ArrayTexture(int width, int height, GLenum internalFormat, int capacity = 0);
    ArrayTexture(const ArrayTexture &other) = delete;
    ArrayTexture &operator=(const ArrayTexture &other) = delete;
    ArrayTexture(ArrayTexture &&other) noexcept;
    ArrayTexture &operator=(ArrayTexture &&other) noexcept;
    ~ArrayTexture();

    int getWidth() const;
    int getHeight() const;
    int getCapacity() const;
    int getCurrentSize() const;
    GLenum getInternalFormat() const;

    // Copy image from render target. Returns layer index
    int insert(RenderTarget &renderTarget);

    // Insert a texture into the array. Returns layer index
    int insert(const gli::texture &texture);

    void addCapacity(int capacity);
    void allocate(bool cubemap, bool textureRepeat, bool mipmaps, float anisotropy);

    void bindTextureUnit(int unit);
    // Intended for uncompressed textures. Calling this with compressed textures is horribly slow
    void generateMipmaps();

  private:
    GLuint id_{0};
    int width_{0};
    int height_{0};
    int current_{0};
    int capacity_{0};
    GLenum internalFormat_{GL_COMPRESSED_RGBA_BPTC_UNORM};

    bool cubemaps_{false};
    bool allocated_{false};

    void validate(const gli::texture &texture);
    void insertIntoArray(const gli::texture &texture, int offset);
};
#pragma once

#include <glad/glad.h>
#include <optional>

#include "renderer/render_target.h"
#include "utilities/ktx_texture.h"

class ArrayTexture
{
  public:
    ArrayTexture() = default;
    ArrayTexture(int width, int height, GLenum internalFormat, bool fallback = false, std::size_t capacity = 0);
    ArrayTexture(const ArrayTexture &other) = delete;
    ArrayTexture &operator=(const ArrayTexture &other) = delete;
    ArrayTexture(ArrayTexture &&other) noexcept;
    ArrayTexture &operator=(ArrayTexture &&other) noexcept;
    ~ArrayTexture();

    int getWidth() const;
    int getHeight() const;
    std::size_t getCapacity() const;
    std::size_t getCurrentSize() const;
    GLenum getInternalFormat() const;
    std::optional<int> getFallbackLayer() const;

    // Copy image from render target. Returns layer index
    std::size_t insert(RenderTarget &renderTarget);

    // Insert a texture into the array. Returns layer index
    std::size_t insert(const KTXTexture &texture);

    void addCapacity(std::size_t capacity);
    void allocate(bool cubemaps, bool textureRepeat, bool mipmaps, float anisotropy);
    void bindTextureUnit(std::uint32_t unit);

    /**
     * @brief Intended for uncompressed textures. Calling this with compressed textures is horribly slow.
     */
    void generateMipmaps();

  private:
    static constexpr int maxArrayTextureLayers_{2048};

    GLuint id_{0};
    int width_{0};
    int height_{0};
    int mipLevels_{0};
    std::size_t current_{0};
    std::size_t capacity_{0};
    GLenum internalFormat_{GL_COMPRESSED_RGBA_BPTC_UNORM};

    bool cubemaps_{false};
    bool allocated_{false};
    bool fallback_{false};

    void validate(const KTXTexture &texture);
    void insertIntoArray(const KTXTexture &texture, std::size_t offset);
    void createFallbackLayer();
    void createSRGBFallback();
    void createRGRGTC2Fallback();
};
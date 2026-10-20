#pragma once

#ifndef KHRONOS_STATIC
#define KHRONOS_STATIC
#endif

#include <filesystem>
#include <glad/glad.h>
#include <ktx.h>

/**
 * @brief RAII wrapper around KTX2 texture.
 */
class KTXTexture
{
  public:
    struct MipData
    {
        const ktx_uint8_t *data{nullptr};
        ktx_size_t size{0};
        GLsizei width{0};
        GLsizei height{0};
    };

    KTXTexture() = default;
    explicit KTXTexture(const std::filesystem::path path);
    KTXTexture(const KTXTexture &other) = delete;
    KTXTexture &operator=(const KTXTexture &other) = delete;
    KTXTexture(KTXTexture &&other) noexcept;
    KTXTexture &operator=(KTXTexture &&other) noexcept;
    ~KTXTexture();

    ktxTexture2 *get() const;

    ktxTexture2 *operator->() const;
    explicit operator bool() const;

    ktx_uint32_t maxLevels() const;
    MipData getMipData(const ktx_uint32_t level, const ktx_uint32_t layer, const ktx_uint32_t face) const;

  private:
    ktxTexture2 *texture_{nullptr};
    std::filesystem::path path_;
    ktx_uint32_t maxMipLevels_{0};
};
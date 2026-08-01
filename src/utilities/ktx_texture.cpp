#pragma once

#include "utilities/ktx_texture.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <ktx.h>
#include <utility>

#include "utilities/texture_utilities.h"

KTXTexture::KTXTexture(const std::filesystem::path path) : path_{path}
{
    KTX_error_code result;
    result = ktxTexture2_CreateFromNamedFile(path.string().c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture_);

    if (result == KTX_SUCCESS)
    {
        if (texture::transcodeKTX(*this))
        {
            int maxMip{texture::calculateMipmapLevels(static_cast<int>(texture_->baseWidth),
                                                               static_cast<int>(texture_->baseHeight), true)};
            maxMipLevels_ = std::min(static_cast<ktx_uint32_t>(maxMip), texture_->numLevels);
        }
        else
        {
            texture_ = nullptr;
        }
    }
    else
    {
        std::cerr << ktxErrorString(result) << std::endl;
        texture_ = nullptr;
    }
}

KTXTexture::KTXTexture(KTXTexture &&other) noexcept
    : texture_{other.texture_}, path_{other.path_}, maxMipLevels_{other.maxMipLevels_}
{
    other.texture_ = nullptr;
}

KTXTexture &KTXTexture::operator=(KTXTexture &&other) noexcept
{
    std::swap(texture_, other.texture_);
    std::swap(path_, other.path_);
    std::swap(maxMipLevels_, other.maxMipLevels_);
    return *this;
}

KTXTexture::~KTXTexture()
{
    if (texture_)
        ktxTexture2_Destroy(texture_);
}

ktxTexture2 *KTXTexture::get() const
{
    return texture_;
}

ktxTexture2 *KTXTexture::operator->() const
{
    return texture_;
}

KTXTexture::operator bool() const
{
    return texture_ != nullptr;
}

ktx_uint32_t KTXTexture::maxLevels() const
{
    return maxMipLevels_;
}

KTXTexture::MipData KTXTexture::getMipData(const ktx_uint32_t level, const ktx_uint32_t layer,
                                           const ktx_uint32_t face) const
{
    if (!texture_)
        return {};

    if (level >= maxMipLevels_)
        return {};

    ktx_size_t offset{0};
    ktx_error_code_e error;
    error = ktxTexture2_GetImageOffset(texture_, level, layer, face, &offset);

    if (error != KTX_SUCCESS)
    {
        std::cerr << "Could not load mip data for " << path_ << std::endl;
        return {};
    }

    return {.data = ktxTexture_GetData(ktxTexture(texture_)) + offset,
            .size = ktxTexture_GetImageSize(ktxTexture(texture_), level),
            .width = static_cast<GLsizei>(texture_->baseWidth >> level),
            .height = static_cast<GLsizei>(texture_->baseHeight >> level)};
}
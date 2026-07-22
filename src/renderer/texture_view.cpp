#include "renderer/texture_view.h"

#include <array>
#include <glad/glad.h>

TextureView::TextureView(GLuint textureID, Sampler sampler, GLenum internalFormat, int mipLevels)
{
    mipViews_.resize(mipLevels);

    for (int i{0}; i < mipLevels; ++i)
    {
        glGenTextures(1, &mipViews_[i]);
        glTextureView(mipViews_[i], GL_TEXTURE_2D, textureID, internalFormat, static_cast<GLuint>(i), 1, 0, 1);

        glTextureParameteri(mipViews_[i], GL_TEXTURE_WRAP_S, sampler.wrapS);
        glTextureParameteri(mipViews_[i], GL_TEXTURE_WRAP_T, sampler.wrapT);
        glTextureParameteri(mipViews_[i], GL_TEXTURE_MIN_FILTER, sampler.minFilter);
        glTextureParameteri(mipViews_[i], GL_TEXTURE_MAG_FILTER, sampler.magFilter);
    }
}

TextureView::TextureView(TextureView &&other) noexcept : mipViews_{std::move(other.mipViews_)}
{
}

TextureView &TextureView::operator=(TextureView &&other) noexcept
{
    if (this != &other)
    {
        for (GLuint id : mipViews_)
            if (id)
                glDeleteTextures(1, &id);

        mipViews_ = other.mipViews_;
        other.mipViews_.clear();
    }

    return *this;
}

TextureView::~TextureView()
{
    for (GLuint id : mipViews_)
        if (id)
            glDeleteTextures(1, &id);
}

const std::vector<GLuint> &TextureView::getMipViews() const
{
    return mipViews_;
}
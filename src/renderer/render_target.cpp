#include "render_target.h"

#include <iostream>

#include "glad/glad.h"

RenderTarget::RenderTarget(int width, int height, Settings settings) : width_{width}, height_{height}
{
    glCreateFramebuffers(1, &FBO_);

    if (settings.msaa)
        createMSAARenderTarget(width, height, settings);
    else
        createRenderTarget(width, height, settings);
}

int calculateMipmapLevels(int width, int height)
{
    int maxDimension = std::max(width, height);
    return static_cast<int>(std::floor(std::log2(maxDimension)) + 1);
}

void RenderTarget::createRenderTarget(int width, int height, const Settings &settings)
{
    int mipLevels{1};
    GLenum textureImageFormat{GL_TEXTURE_2D};

    if (settings.textureArray)
        textureImageFormat = GL_TEXTURE_2D_ARRAY;

    if (settings.generateMipmaps)
        mipLevels = calculateMipmapLevels(width, height);

    glCreateTextures(textureImageFormat, 1, &colorTextureID_);

    if (settings.textureArray)
        glTextureStorage3D(colorTextureID_, mipLevels, settings.internalFormat, width, height,
                           settings.textureArrayDepth);
    else
        glTextureStorage2D(colorTextureID_, mipLevels, settings.internalFormat, width, height);

    glTextureParameteri(colorTextureID_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(colorTextureID_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(colorTextureID_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(colorTextureID_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glNamedFramebufferTexture(FBO_, GL_COLOR_ATTACHMENT0, colorTextureID_, 0);

    if (settings.depthType == DepthType::TEXTURE)
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &depthMapTextureID_);
        glTextureStorage2D(depthMapTextureID_, 1, settings.depthFormat, width, height);

        glTextureParameteri(depthMapTextureID_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(depthMapTextureID_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(depthMapTextureID_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(depthMapTextureID_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glNamedFramebufferTexture(FBO_, GL_DEPTH_ATTACHMENT, depthMapTextureID_, 0);
    }
    else if (settings.depthType == DepthType::RBO)
    {
        glCreateRenderbuffers(1, &depthRBO_);
        glNamedRenderbufferStorage(depthRBO_, settings.depthFormat, width, height);
        glNamedFramebufferRenderbuffer(FBO_, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRBO_);
    }

    if (glCheckNamedFramebufferStatus(FBO_, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR: failed to create framebuffer" << std::endl;
}

void RenderTarget::createMSAARenderTarget(int width, int height, const Settings &settings)
{
    // Color
    if (settings.textureArray)
    {
        glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 1, &colorTextureID_);
        glTextureStorage3DMultisample(colorTextureID_, settings.msaa, settings.internalFormat, width, height,
                                      settings.textureArrayDepth, GL_TRUE);
    }
    else
    {
        glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &colorTextureID_);
        glTextureStorage2DMultisample(colorTextureID_, settings.msaa, settings.internalFormat, width, height, GL_TRUE);
    }

    glNamedFramebufferTexture(FBO_, GL_COLOR_ATTACHMENT0, colorTextureID_, 0);

    if (settings.depthType == DepthType::TEXTURE)
    { // Depth texture
        glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &depthMapTextureID_);
        glTextureStorage2DMultisample(depthMapTextureID_, settings.msaa, settings.depthFormat, width, height, GL_TRUE);
        glNamedFramebufferTexture(FBO_, GL_DEPTH_ATTACHMENT, depthMapTextureID_, 0);
    }
    else if (settings.depthType == DepthType::RBO)
    { // Depth RBO
        glCreateRenderbuffers(1, &depthRBO_);
        glNamedRenderbufferStorageMultisample(depthRBO_, settings.msaa, settings.depthFormat, width, height);
        glNamedFramebufferRenderbuffer(FBO_, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRBO_);
    }

    if (glCheckNamedFramebufferStatus(FBO_, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR: failed to create framebuffer" << std::endl;
}

RenderTarget::RenderTarget(RenderTarget &&other) noexcept
    : width_{other.width_}, height_{other.height_}, FBO_{other.FBO_}, depthRBO_{other.depthRBO_},
      colorTextureID_{other.colorTextureID_}, depthMapTextureID_{other.depthMapTextureID_}
{
    other.FBO_ = 0;
    other.depthRBO_ = 0;
    other.colorTextureID_ = 0;
    other.depthMapTextureID_ = 0;
}

RenderTarget &RenderTarget::operator=(RenderTarget &&other) noexcept
{
    if (this != &other)
    {
        deleteResources();

        width_ = other.width_;
        height_ = other.height_;
        FBO_ = other.FBO_;
        depthRBO_ = other.depthRBO_;
        colorTextureID_ = other.colorTextureID_;
        depthMapTextureID_ = other.depthMapTextureID_;

        other.FBO_ = 0;
        other.depthRBO_ = 0;
        other.colorTextureID_ = 0;
        other.depthMapTextureID_ = 0;
    }

    return *this;
}

RenderTarget::~RenderTarget()
{
    deleteResources();
}

void RenderTarget::bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, FBO_);
    glViewport(0, 0, width_, height_);
}

void RenderTarget::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint RenderTarget::getDepthMapTextureID() const
{
    return depthMapTextureID_;
}

void RenderTarget::generateMipMaps()
{
    glGenerateTextureMipmap(colorTextureID_);
}

GLuint RenderTarget::getColorTextureID() const
{
    return colorTextureID_;
}

int RenderTarget::getWidth() const
{
    return width_;
}

int RenderTarget::getHeight() const
{
    return height_;
}

void RenderTarget::clear(bool color, bool depth)
{
    if (color)
        glClearNamedFramebufferfv(FBO_, GL_COLOR, 0, clearColor.data());
    if (depth)
        glClearNamedFramebufferfv(FBO_, GL_DEPTH, 0, &depthColor);
}

void RenderTarget::deleteResources()
{
    glDeleteTextures(1, &colorTextureID_);

    if (depthMapTextureID_)
        glDeleteTextures(1, &depthMapTextureID_);
    if (depthRBO_)
        glDeleteRenderbuffers(1, &depthRBO_);

    glDeleteFramebuffers(1, &FBO_);
}

void RenderTarget::blitTo(const RenderTarget &target)
{
    glBlitNamedFramebuffer(FBO_, target.FBO_, 0, 0, width_, height_, 0, 0, target.width_, target.height_,
                           GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

void RenderTarget::setTextureLayer(GLuint layer)
{
    glNamedFramebufferTextureLayer(FBO_, GL_COLOR_ATTACHMENT0, colorTextureID_, 0, layer);
}
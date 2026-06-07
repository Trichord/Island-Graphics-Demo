#include "renderer/render_target.h"

#include <glad/glad.h>
#include <iostream>
#include <stdexcept>
#include <utility>

#include "utilities/texture_utilities.h"

RenderTarget::RenderTarget(int width, int height, Settings settings)
    : layers_{settings.textureArrayDepth}, width_{width}, height_{height}, internalFormat_{settings.internalFormat}
{
    if (settings.depthType == DepthType::RBO && settings.textureArray == true)
    {
        std::cerr << "ERROR: Cannot create layered framebuffer with only a depth Render Buffer Object" << std::endl;
        throw std::runtime_error("Framebuffer Creation Error");
    }

    glCreateFramebuffers(1, &FBO_);

    if (settings.msaa)
        createMSAARenderTarget(width, height, settings);
    else
        createRenderTarget(width, height, settings);

    if (settings.internalFormat == GL_NONE)
    {
        glNamedFramebufferDrawBuffer(FBO_, GL_NONE);
        glNamedFramebufferReadBuffer(FBO_, GL_NONE);
    }
    else
    {
        glNamedFramebufferDrawBuffer(FBO_, GL_COLOR_ATTACHMENT0);
        glNamedFramebufferReadBuffer(FBO_, GL_COLOR_ATTACHMENT0);
    }

    GLenum status = glCheckNamedFramebufferStatus(FBO_, GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "ERROR: framebuffer incomplete, status = 0x" << status << "\n";
        throw std::runtime_error("Framebuffer incomplete");
    }
}

void RenderTarget::createRenderTarget(int width, int height, const Settings &settings)
{
    int mipLevels{1};
    GLenum target{GL_TEXTURE_2D};

    if (settings.textureArray)
        target = GL_TEXTURE_2D_ARRAY;

    if (settings.generateMipmaps)
        mipLevels = TextureUtilities::calculateMipmapLevels(width, height, false);

    if (settings.internalFormat != GL_NONE)
        createColorTexture(width, height, mipLevels, target, settings);

    if (settings.depthType == DepthType::texture)
        createDepthTexture(width, height, target, settings);
    else if (settings.depthType == DepthType::RBO)
        createRBO(width, height, settings);
}

void RenderTarget::createColorTexture(int width, int height, int mipLevels, GLenum target, const Settings &settings)
{
    glCreateTextures(target, 1, &colorTextureID_);

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
}

void RenderTarget::createDepthTexture(int width, int height, GLenum target, const Settings &settings)
{
    glCreateTextures(target, 1, &depthMapTextureID_);

    if (settings.textureArray)
        glTextureStorage3D(depthMapTextureID_, 1, settings.depthFormat, width, height, settings.textureArrayDepth);
    else
        glTextureStorage2D(depthMapTextureID_, 1, settings.depthFormat, width, height);

    if (settings.shadowMap)
    {
        glTextureParameteri(depthMapTextureID_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTextureParameteri(depthMapTextureID_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTextureParameteri(depthMapTextureID_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(depthMapTextureID_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(depthMapTextureID_, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTextureParameteri(depthMapTextureID_, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

        float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTextureParameterfv(depthMapTextureID_, GL_TEXTURE_BORDER_COLOR, borderColor);
    }

    else
    {
        glTextureParameteri(depthMapTextureID_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(depthMapTextureID_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(depthMapTextureID_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(depthMapTextureID_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    glNamedFramebufferTexture(FBO_, GL_DEPTH_ATTACHMENT, depthMapTextureID_, 0);
}
void RenderTarget::createRBO(int width, int height, const Settings &settings)
{
    glCreateRenderbuffers(1, &depthRBO_);
    glNamedRenderbufferStorage(depthRBO_, settings.depthFormat, width, height);
    glNamedFramebufferRenderbuffer(FBO_, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRBO_);
}

void RenderTarget::createMSAARenderTarget(int width, int height, const Settings &settings)
{
    if (settings.internalFormat != GL_NONE)
        createMultisampleColorTexture(width, height, settings);

    if (settings.depthType == DepthType::texture)
        createMultisampleTexture(width, height, settings);
    else if (settings.depthType == DepthType::RBO)
        createMultisampleRBO(width, height, settings);
}

void RenderTarget::createMultisampleColorTexture(int width, int height, const Settings &settings)
{
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
}

void RenderTarget::createMultisampleRBO(int width, int height, const Settings &settings)
{
    glCreateRenderbuffers(1, &depthRBO_);
    glNamedRenderbufferStorageMultisample(depthRBO_, settings.msaa, settings.depthFormat, width, height);
    glNamedFramebufferRenderbuffer(FBO_, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRBO_);
}

void RenderTarget::createMultisampleTexture(int width, int height, const Settings &settings)
{
    if (settings.textureArray)
    {
        glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 1, &depthMapTextureID_);
        glTextureStorage3DMultisample(depthMapTextureID_, settings.msaa, settings.depthFormat, width, height,
                                      settings.textureArrayDepth, GL_TRUE);
    }
    else
    {
        glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &depthMapTextureID_);
        glTextureStorage2DMultisample(depthMapTextureID_, settings.msaa, settings.depthFormat, width, height, GL_TRUE);
    }

    glNamedFramebufferTexture(FBO_, GL_DEPTH_ATTACHMENT, depthMapTextureID_, 0);
}

RenderTarget::RenderTarget(RenderTarget &&other) noexcept
    : layers_{other.layers_}, width_{other.width_}, height_{other.height_}, FBO_{other.FBO_},
      depthRBO_{other.depthRBO_}, colorTextureID_{other.colorTextureID_}, depthMapTextureID_{other.depthMapTextureID_},
      internalFormat_{other.internalFormat_}
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
        std::swap(layers_, other.layers_);
        std::swap(width_, other.width_);
        std::swap(height_, other.height_);
        std::swap(FBO_, other.FBO_);
        std::swap(depthRBO_, other.depthRBO_);
        std::swap(colorTextureID_, other.colorTextureID_);
        std::swap(depthMapTextureID_, other.depthMapTextureID_);
        std::swap(internalFormat_, other.internalFormat_);
    }

    return *this;
}

RenderTarget::~RenderTarget()
{
    if (FBO_ == 0)
        return;

    if (colorTextureID_)
        glDeleteTextures(1, &colorTextureID_);

    if (depthMapTextureID_)
        glDeleteTextures(1, &depthMapTextureID_);
    if (depthRBO_)
        glDeleteRenderbuffers(1, &depthRBO_);

    glDeleteFramebuffers(1, &FBO_);
}

void RenderTarget::bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, FBO_);
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

int RenderTarget::getNumLayers() const
{
    return layers_;
}

GLenum RenderTarget::getInternalFormat() const
{
    return internalFormat_;
}

void RenderTarget::clear(bool color, bool depth)
{
    if (color)
        glClearNamedFramebufferfv(FBO_, GL_COLOR, 0, clearColor.data());
    if (depth)
        glClearNamedFramebufferfv(FBO_, GL_DEPTH, 0, &depthColor);
}

void RenderTarget::blitTo(const RenderTarget &target)
{
    glBlitNamedFramebuffer(FBO_, target.FBO_, 0, 0, width_, height_, 0, 0, target.width_, target.height_,
                           GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

void RenderTarget::setTextureLayer(int layer)
{
    if (layer >= layers_)
    {
        std::cerr << "ERROR: RenderTarget " << layer << " out of bounds for size " << layers_ << std::endl;
        throw std::runtime_error("RenderTarget layer out of bounds");
    }

    if (colorTextureID_ != 0)
        glNamedFramebufferTextureLayer(FBO_, GL_COLOR_ATTACHMENT0, colorTextureID_, 0, static_cast<GLuint>(layer));

    if (depthMapTextureID_ != 0)
        glNamedFramebufferTextureLayer(FBO_, GL_DEPTH_ATTACHMENT, depthMapTextureID_, 0, static_cast<GLuint>(layer));
}
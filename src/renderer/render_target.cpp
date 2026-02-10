#include "render_target.h"

#include <iostream>

#include "glad/glad.h"

RenderTarget::RenderTarget(int width, int height, int msaaLevel, bool depthMapTexture) : width{width}, height{height}
{
    glCreateFramebuffers(1, &FBO);

    if (msaaLevel)
    {
        // Color
        glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &colorTextureID);
        glTextureStorage2DMultisample(colorTextureID, msaaLevel, GL_SRGB8_ALPHA8, width, height, GL_TRUE);
        glNamedFramebufferTexture(FBO, GL_COLOR_ATTACHMENT0, colorTextureID, 0);

        if (depthMapTexture)
        { // Depth texture
            glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &depthMapTextureID);
            glTextureStorage2DMultisample(depthMapTextureID, msaaLevel, GL_DEPTH_COMPONENT24, width, height, GL_TRUE);
            glNamedFramebufferTexture(FBO, GL_DEPTH_ATTACHMENT, depthMapTextureID, 0);
        }
        else
        { // Depth RBO
            glCreateRenderbuffers(1, &depthRBO);
            glNamedRenderbufferStorageMultisample(depthRBO, msaaLevel, GL_DEPTH_COMPONENT24, width, height);
            glNamedFramebufferRenderbuffer(FBO, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRBO);
        }

        if (glCheckNamedFramebufferStatus(FBO, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "ERROR: failed to create framebuffer" << std::endl;

        return;
    }

    // Non MSAA FBO
    glCreateTextures(GL_TEXTURE_2D, 1, &colorTextureID);
    glTextureStorage2D(colorTextureID, 1, GL_SRGB8_ALPHA8, width, height);
    glTextureParameteri(colorTextureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(colorTextureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(colorTextureID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(colorTextureID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glNamedFramebufferTexture(FBO, GL_COLOR_ATTACHMENT0, colorTextureID, 0);

    if (depthMapTexture)
    {
        glCreateTextures(GL_TEXTURE_2D, 1, &depthMapTextureID);
        glTextureStorage2D(depthMapTextureID, 1, GL_DEPTH_COMPONENT24, width, height);
        glTextureParameteri(depthMapTextureID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(depthMapTextureID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(depthMapTextureID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(depthMapTextureID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glNamedFramebufferTexture(FBO, GL_DEPTH_ATTACHMENT, depthMapTextureID, 0);
    }
    else
    {
        glCreateRenderbuffers(1, &depthRBO);
        glNamedRenderbufferStorage(depthRBO, GL_DEPTH_COMPONENT24, width, height);
        glNamedFramebufferRenderbuffer(FBO, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRBO);
    }

    if (glCheckNamedFramebufferStatus(FBO, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR: failed to create framebuffer" << std::endl;
}

RenderTarget::RenderTarget(RenderTarget &&other) noexcept
    : width{other.width}, height{other.height}, FBO{other.FBO}, depthRBO{other.depthRBO},
      colorTextureID{other.colorTextureID}, depthMapTextureID{other.depthMapTextureID}
{
    other.FBO = 0;
    other.depthRBO = 0;
    other.colorTextureID = 0;
    other.depthMapTextureID = 0;
}

RenderTarget &RenderTarget::operator=(RenderTarget &&other) noexcept
{
    if (this != &other)
    {
        deleteResources();

        width = other.width;
        height = other.height;
        FBO = other.FBO;
        depthRBO = other.depthRBO;
        colorTextureID = other.colorTextureID;
        depthMapTextureID = other.depthMapTextureID;

        other.FBO = 0;
        other.depthRBO = 0;
        other.colorTextureID = 0;
        other.depthMapTextureID = 0;
    }

    return *this;
}

RenderTarget::~RenderTarget()
{
    deleteResources();
}

void RenderTarget::bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glViewport(0, 0, width, height);
}

void RenderTarget::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint RenderTarget::getDepthMapTextureID() const
{
    return depthMapTextureID;
}

void RenderTarget::generateMipMaps()
{
    glGenerateTextureMipmap(colorTextureID);
}

GLuint RenderTarget::getColorTextureID() const
{
    return colorTextureID;
}

int RenderTarget::getWidth() const
{
    return width;
}

int RenderTarget::getHeight() const
{
    return height;
}

void RenderTarget::clear(bool color, bool depth)
{
    if (color)
        glClearNamedFramebufferfv(FBO, GL_COLOR, 0, clearColor.data());
    if (depth)
        glClearNamedFramebufferfv(FBO, GL_DEPTH, 0, &depthColor);
}

void RenderTarget::deleteResources()
{
    glDeleteTextures(1, &colorTextureID);

    if (depthMapTextureID)
        glDeleteTextures(1, &depthMapTextureID);
    if (depthRBO)
        glDeleteRenderbuffers(1, &depthRBO);

    glDeleteFramebuffers(1, &FBO);
}

void RenderTarget::resolveTo(const RenderTarget &target)
{
    glBlitNamedFramebuffer(FBO, target.FBO, 0, 0, width, height, 0, 0, target.width, target.height, GL_COLOR_BUFFER_BIT,
                           GL_LINEAR);
}
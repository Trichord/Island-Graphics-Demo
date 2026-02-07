#pragma once

#include <array>

#include "glad/glad.h"

class RenderTarget
{
  public:
    RenderTarget(int width, int height, int msaaLevel, bool depthMapTexture = true);
    RenderTarget(const RenderTarget &other) = delete;
    RenderTarget &operator=(const RenderTarget &) = delete;
    RenderTarget(RenderTarget &&other) noexcept;
    RenderTarget &operator=(RenderTarget &&other) noexcept;
    ~RenderTarget();

    void bind();
    void unbind();
    int getWidth() const;
    int getHeight() const;
    void generateMipMaps();
    GLuint getColorTextureID() const;
    GLuint getDepthMapTextureID() const;
    void resolveTo(const RenderTarget &target);
    void clear(bool color = true, bool depth = true);

  private:
    int width{960};
    int height{540};
    inline static const std::array<GLfloat, 4> clearColor{0.0f, 0.0f, 0.0f, 0.0f};
    inline static const GLfloat depthColor{1.0f};

    GLuint FBO{0};
    GLuint depthRBO{0};
    GLuint colorTextureID{0};
    GLuint depthMapTextureID{0};

    void deleteResources();
};
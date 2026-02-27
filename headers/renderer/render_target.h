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
    static constexpr float depthColor{1.0f};
    static constexpr std::array<float, 4> clearColor{0.0f, 0.0f, 0.0f, 1.0f};
    
    int width_{960};
    int height_{540};

    GLuint FBO_{0};
    GLuint depthRBO_{0};
    GLuint colorTextureID_{0};
    GLuint depthMapTextureID_{0};

    void deleteResources();
};
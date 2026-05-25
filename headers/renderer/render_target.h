#pragma once

#include <array>
#include <glad/glad.h>

class RenderTarget
{
  public:
    enum class DepthType
    {
        NONE,
        RBO,
        TEXTURE
    };

    struct Settings
    {
        int msaa{2};
        DepthType depthType{DepthType::RBO};
        GLenum internalFormat{GL_SRGB8};
        GLenum depthFormat{GL_DEPTH_COMPONENT24};
        bool textureArray{false};
        GLsizei textureArrayDepth{1};
        bool generateMipmaps{false};
    };

    RenderTarget(int width, int height, Settings settings);
    RenderTarget(const RenderTarget &other) = delete;
    RenderTarget &operator=(const RenderTarget &) = delete;
    RenderTarget(RenderTarget &&other) noexcept;
    RenderTarget &operator=(RenderTarget &&other) noexcept;
    ~RenderTarget();

    void bind();
    void unbind();

    int getWidth() const;
    int getHeight() const;
    int getNumLayers() const;
    GLenum getInternalFormat() const;
    GLuint getColorTextureID() const;
    GLuint getDepthMapTextureID() const;

    void setTextureLayer(int layer);

    void generateMipMaps();
    void blitTo(const RenderTarget &target);
    void clear(bool color = true, bool depth = true);

  private:
    static constexpr float depthColor{1.0f};
    static constexpr std::array<float, 4> clearColor{0.0f, 0.0f, 0.0f, 1.0f};

    int layers_{1};
    int width_{1920};
    int height_{1080};
    
    GLuint FBO_{0};
    GLuint depthRBO_{0};
    GLuint colorTextureID_{0};
    GLuint depthMapTextureID_{0};
    GLenum internalFormat_{GL_SRGB8};

    void createRenderTarget(int width, int height, const Settings &settings);
    void createMSAARenderTarget(int width, int height, const Settings &settings);
};
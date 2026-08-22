#pragma once

#include <array>
#include <glad/glad.h>

class RenderTarget
{
  public:
    enum class DepthType
    {
        none,
        RBO,
        texture
    };

    struct Settings
    {
        int msaa{0};
        DepthType depthType{DepthType::none};
        GLenum internalFormat{GL_NONE};
        GLenum depthFormat{GL_NONE};
        bool textureArray{false};
        int textureArrayDepth{1};
        bool mipmaps{false};
        bool shadowMap{false};
    };

    RenderTarget() = default;
    RenderTarget(int width, int height, Settings settings);
    RenderTarget(const RenderTarget &other) = delete;
    RenderTarget &operator=(const RenderTarget &) = delete;
    RenderTarget(RenderTarget &&other) noexcept;
    RenderTarget &operator=(RenderTarget &&other) noexcept;
    ~RenderTarget();

    GLuint getID() const;
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
    static constexpr std::array<float, 4> clearColor{1.0f, 1.0f, 1.0f, 1.0f};

    GLsizei layers_{1};
    GLsizei width_{1920};
    GLsizei height_{1080};

    GLuint FBO_{0};
    GLuint depthRBO_{0};
    GLuint colorTextureID_{0};
    GLuint depthMapTextureID_{0};
    GLenum internalFormat_{GL_NONE};

    void createRenderTarget(const Settings &settings);
    void createMSAARenderTarget(const Settings &settings);

    void createRBO(const Settings &settings);
    void createDepthTexture(GLenum target, const Settings &settings);
    void createColorTexture(int mipLevels, GLenum target, const Settings &settings);

    void createMultisampleRBO(const Settings &settings);
    void createMultisampleTexture(const Settings &settings);
    void createMultisampleColorTexture(const Settings &settings);
};
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
        GLsizei textureArrayDepth{1};
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
    static constexpr std::array<float, 4> clearColor{1.0f, 1.0f, 1.0f, 1.0f};

    int layers_{1};
    int width_{1920};
    int height_{1080};

    GLuint FBO_{0};
    GLuint depthRBO_{0};
    GLuint colorTextureID_{0};
    GLuint depthMapTextureID_{0};
    GLenum internalFormat_{GL_NONE};

    void createRenderTarget(int width, int height, const Settings &settings);
    void createMSAARenderTarget(int width, int height, const Settings &settings);

    void createRBO(int width, int height, const Settings &settings);
    void createDepthTexture(int width, int height, GLenum target, const Settings &settings);
    void createColorTexture(int width, int height, int mipLevels, GLenum target, const Settings &settings);

    void createMultisampleRBO(int width, int height, const Settings &settings);
    void createMultisampleTexture(int width, int height, const Settings &settings);
    void createMultisampleColorTexture(int width, int height, const Settings &settings);
};
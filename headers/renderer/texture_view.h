#pragma once

#include "renderer/textures.h"
#include <glad/glad.h>
#include <vector>

class TextureView
{
  public:
    TextureView() = default;
    TextureView(GLuint textureID, Sampler sampler, GLenum internalFormat, int mipLevels);
    TextureView(const TextureView &other) = delete;
    TextureView &operator=(const TextureView &other) = delete;
    TextureView(TextureView &&other) noexcept;
    TextureView &operator=(TextureView &&other) noexcept;
    ~TextureView();

    const std::vector<GLuint> &getMipViews() const;

  private:
    std::vector<GLuint> mipViews_;
};
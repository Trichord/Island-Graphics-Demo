#pragma once

#include <vector>

#include "fastgltf/types.hpp"
#include "glad/glad.h"

class GLState
{
  public:
    GLState();

    void bindTexture(GLuint unit, GLuint id);
    void setAlphaMode(fastgltf::AlphaMode mode);
    void setDepthFunction(GLenum depthFunction);

  private:
    bool a2cEnabled_{false};
    bool blendEnabled_{false};
    bool cullFaceEnabled_{false};

    GLenum currentDepthFunc_{GL_LESS};

    std::vector<GLuint> textureUnits_;
};
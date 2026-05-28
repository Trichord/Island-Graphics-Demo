#pragma once

#include <fastgltf/types.hpp>
#include <glad/glad.h>
#include <vector>

class GLState
{
  public:
    GLState();

    void bindTexture(GLuint unit, GLuint id);
    void setAlphaMode(fastgltf::AlphaMode mode);
    void setDepthFunction(GLenum depthFunction);
    void setVAO(GLint vao);

  private:
    bool a2cEnabled_{false};
    bool blendEnabled_{false};
    bool cullFaceEnabled_{false};

    GLenum currentDepthFunc_{GL_LESS};

    GLuint currentVAO_{0};

    std::vector<GLuint> textureUnits_;
};
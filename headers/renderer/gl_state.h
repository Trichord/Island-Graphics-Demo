#pragma once

#include <fastgltf/types.hpp>
#include <glad/glad.h>
#include <vector>

#include "models/primitive.h"
#include "renderer/shader/shader_base.h"

class GLState
{
  public:
    GLState();

    void bindProgram(Shader &shader);
    void bindTexture(GLuint unit, GLuint id);
    void setAlphaMode(AlphaMode mode);
    void setDepthFunction(GLenum depthFunction);
    void setVAO(GLint vao);

  private:
    bool a2cEnabled_{false};
    bool blendEnabled_{false};
    bool cullFaceEnabled_{false};

    GLenum currentDepthFunc_{GL_LESS};
    GLuint currentProgram_{0};
    GLuint currentVAO_{0};

    std::vector<GLuint> textureUnits_;
};
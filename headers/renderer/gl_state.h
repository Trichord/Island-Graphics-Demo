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

    void setVAO(GLuint vao);
    void setFaceCull(GLenum mode);
    void setAlphaMode(AlphaMode mode);
    void setDepthWrite(bool enableDepth);
    void setDepthFunction(GLenum depthFunction);

    void bindTexture(GLuint unit, GLuint id);
    void bindProgram(Shader &shader);

  private:
    bool a2cEnabled_{false};
    bool blendEnabled_{false};
    bool cullFaceEnabled_{false};
    bool depthWriteEnabled_{true};

    GLenum currentFaceCull_{GL_BACK};
    GLenum currentDepthFunc_{GL_LESS};
    GLuint currentProgram_{0};
    GLuint currentVAO_{0};

    std::vector<GLuint> textureUnits_;
};
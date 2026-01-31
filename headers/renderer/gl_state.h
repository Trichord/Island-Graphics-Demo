#pragma once

#include <vector>

#include "fastgltf/types.hpp"
#include "glad/glad.h"

class GLState
{
  public:
    GLState(bool globalMSAA);

    void bindTexture(GLuint unit, GLuint id);
    void setAlphaMode(fastgltf::AlphaMode mode);

  private:
    bool alphaEnabled{false};
    const bool globalMSAAEnabled{false};

    std::vector<GLuint> textureUnits;
};
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

  private:
    bool alphaMaskEnabled{false};
    bool alphaBlendEnabled{false};
    bool cullFaceEnabled{false};

    std::vector<GLuint> textureUnits;
};
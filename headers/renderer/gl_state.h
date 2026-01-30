#pragma once

#include <vector>

#include "glad/glad.h"
#include "fastgltf/types.hpp"

class GLState {
 public:
  GLState(bool globalMSAA);

  void bindTexture(GLuint unit, GLuint id);
  void setAlphaMode(fastgltf::AlphaMode mode);

 private:  
  bool alphaEnabled{false};
  const bool globalMSAAEnabled{false};

  std::vector<GLuint> textureUnits;
};
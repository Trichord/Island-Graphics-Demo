#pragma once

#include <vector>

#include "glad/glad.h"

class GLState {
 public:
  GLState(bool globalMSAA);

  void bindTexture(GLuint unit, GLuint id);
  void setAlphaMode(bool enable);

 private:  
  bool alphaEnabled{false};
  const bool globalMSAAEnabled{false};

  GLint maxTextureUnits;
  std::vector<GLuint> textureUnits;
};
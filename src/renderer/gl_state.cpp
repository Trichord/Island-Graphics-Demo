#include "gl_state.h"

#include <stdexcept>

#include "glad/glad.h"

GLState::GLState(bool globalMSAA) : globalMSAAEnabled{globalMSAA} {
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
  textureUnits.resize(maxTextureUnits);
}

void GLState::bindTexture(GLuint unit, GLuint id) {
  if (unit >= maxTextureUnits)
    throw std::runtime_error("Max texture units exceeded.");

  if (textureUnits[unit] == id) return;

  glBindTextureUnit(unit, id);
  textureUnits[unit] = id;
}

void GLState::setAlphaMode(bool enable) {
  if (enable == alphaEnabled) return;

  if (enable) {
    glDisable(GL_CULL_FACE);
    alphaEnabled = true;

    if (globalMSAAEnabled) glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
  } else {
    glEnable(GL_CULL_FACE);
    alphaEnabled = false;

    if (globalMSAAEnabled) glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
  }
}
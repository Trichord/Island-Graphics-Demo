#include "gl_state.h"

#include <stdexcept>

#include "glad/glad.h"

constexpr GLuint maxTextureUnits{16u};

GLState::GLState()
{
    textureUnits.resize(maxTextureUnits);
}

void GLState::bindTexture(GLuint unit, GLuint id)
{
    if (unit >= maxTextureUnits)
        throw std::runtime_error("Max texture units exceeded.");

    if (textureUnits[unit] == id)
        return;

    glBindTextureUnit(unit, id);
    textureUnits[unit] = id;
}

void GLState::setAlphaMode(fastgltf::AlphaMode mode)
{
    bool enable{mode == fastgltf::AlphaMode::Blend || mode == fastgltf::AlphaMode::Mask};

    if (enable == alphaEnabled)
        return;

    if (enable)
    {
        glDisable(GL_CULL_FACE);
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        alphaEnabled = true;
    }
    else
    {
        glEnable(GL_CULL_FACE);
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        alphaEnabled = false;
    }
}
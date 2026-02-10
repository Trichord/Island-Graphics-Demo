#include "gl_state.h"

#include <stdexcept>

#include "fastgltf/types.hpp"
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
    using namespace fastgltf;
    using enum AlphaMode;

    if (mode == Blend && alphaBlendEnabled)
        return;
    else if (mode == Mask && alphaMaskEnabled)
        return;
    else if (mode == Opaque && !(alphaBlendEnabled || alphaMaskEnabled))
        return;

    switch (mode)
    {
    case Opaque:
        glEnable(GL_CULL_FACE);

        if (alphaMaskEnabled)
            glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

        if (alphaBlendEnabled)
            glDisable(GL_BLEND);

        cullFaceEnabled = true;
        alphaMaskEnabled = false;
        alphaBlendEnabled = false;
        break;

    case Mask:
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);

        if (cullFaceEnabled)
            glDisable(GL_CULL_FACE);

        if (alphaBlendEnabled)
            glDisable(GL_BLEND);

        cullFaceEnabled = false;
        alphaMaskEnabled = true;
        alphaBlendEnabled = false;
        break;

    case Blend:
        glEnable(GL_BLEND);

        if (cullFaceEnabled)
            glDisable(GL_CULL_FACE);

        if (alphaMaskEnabled)
            glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

        cullFaceEnabled = false;
        alphaMaskEnabled = false;
        alphaBlendEnabled = true;
        break;
    }
}
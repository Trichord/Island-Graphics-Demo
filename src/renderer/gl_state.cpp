#include "gl_state.h"

#include <stdexcept>

#include "fastgltf/types.hpp"
#include "glad/glad.h"

constexpr GLuint maxTextureUnits{16u};

GLState::GLState()
{
    textureUnits_.resize(maxTextureUnits);
}

void GLState::bindTexture(GLuint unit, GLuint id)
{
    if (unit >= maxTextureUnits)
        throw std::runtime_error("Max texture units exceeded.");

    if (textureUnits_[unit] == id)
        return;

    glBindTextureUnit(unit, id);
    textureUnits_[unit] = id;
}

void GLState::setAlphaMode(fastgltf::AlphaMode mode)
{
    using namespace fastgltf;
    using enum AlphaMode;

    if (mode == Blend && alphaBlendEnabled_)
        return;
    else if (mode == Mask && alphaMaskEnabled_)
        return;
    else if (mode == Opaque && !(alphaBlendEnabled_ || alphaMaskEnabled_))
        return;

    switch (mode)
    {
    case Opaque:
        glEnable(GL_CULL_FACE);

        if (alphaMaskEnabled_)
            glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

        if (alphaBlendEnabled_)
            glDisable(GL_BLEND);

        cullFaceEnabled_ = true;
        alphaMaskEnabled_ = false;
        alphaBlendEnabled_ = false;
        break;

    case Mask:
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);

        if (cullFaceEnabled_)
            glDisable(GL_CULL_FACE);

        if (alphaBlendEnabled_)
            glDisable(GL_BLEND);

        cullFaceEnabled_ = false;
        alphaMaskEnabled_ = true;
        alphaBlendEnabled_ = false;
        break;

    case Blend:
        glEnable(GL_BLEND);

        if (cullFaceEnabled_)
            glDisable(GL_CULL_FACE);

        if (alphaMaskEnabled_)
            glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

        cullFaceEnabled_ = false;
        alphaMaskEnabled_ = false;
        alphaBlendEnabled_ = true;
        break;
    }
}
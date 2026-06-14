#include "renderer/gl_state.h"

#include <fastgltf/types.hpp>
#include <glad/glad.h>
#include <stdexcept>

#include "renderer/shader/shader_base.h"
#include "models/primitive.h"

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

void GLState::setAlphaMode(AlphaMode mode)
{
    using enum AlphaMode;

    if (mode == Blend && blendEnabled_)
        return;
    else if (mode == Mask && a2cEnabled_)
        return;
    else if (mode == Opaque && !(blendEnabled_ || a2cEnabled_))
        return;

    switch (mode)
    {
    case Opaque:
        glEnable(GL_CULL_FACE);

        if (a2cEnabled_)
            glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

        if (blendEnabled_)
            glDisable(GL_BLEND);

        cullFaceEnabled_ = true;
        a2cEnabled_ = false;
        blendEnabled_ = false;
        break;

    case Mask:
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);

        if (cullFaceEnabled_)
            glDisable(GL_CULL_FACE);

        if (blendEnabled_)
            glDisable(GL_BLEND);

        cullFaceEnabled_ = false;
        a2cEnabled_ = true;
        blendEnabled_ = false;
        break;

    case Blend:
        glEnable(GL_BLEND);

        if (cullFaceEnabled_)
            glDisable(GL_CULL_FACE);

        if (a2cEnabled_)
            glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

        cullFaceEnabled_ = false;
        a2cEnabled_ = false;
        blendEnabled_ = true;
        break;
    }
}

void GLState::setDepthFunction(GLenum depthFunction)
{
    if (currentDepthFunc_ == depthFunction)
        return;

    currentDepthFunc_ = depthFunction;
    glDepthFunc(depthFunction);
}

void GLState::setVAO(GLint vao)
{
    if (vao == currentVAO_)
        return;

    glBindVertexArray(vao);
    currentVAO_ = vao;
}

void GLState::bindProgram(Shader &shader)
{
    const GLuint id{shader.getID()};

    if (id == currentProgram_)
        return;

    shader.use();
    currentProgram_ = id;
}
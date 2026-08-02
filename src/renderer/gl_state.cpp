#include "renderer/gl_state.h"

#include <fastgltf/types.hpp>
#include <glad/glad.h>
#include <stdexcept>

#include "models/primitive.h"
#include "renderer/shader/shader_base.h"

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

    if (mode == blend && blendEnabled_)
        return;
    else if (mode == mask && a2cEnabled_)
        return;
    else if (mode == opaque && !(blendEnabled_ || a2cEnabled_))
        return;

    switch (mode)
    {
    case opaque:
        glEnable(GL_CULL_FACE);

        if (a2cEnabled_)
            glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

        if (blendEnabled_)
            glDisable(GL_BLEND);

        cullFaceEnabled_ = true;
        a2cEnabled_ = false;
        blendEnabled_ = false;
        break;

    case mask:
        glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);

        if (cullFaceEnabled_)
            glDisable(GL_CULL_FACE);

        if (blendEnabled_)
            glDisable(GL_BLEND);

        cullFaceEnabled_ = false;
        a2cEnabled_ = true;
        blendEnabled_ = false;
        break;

    case blend:
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

void GLState::setDepthWrite(bool enableDepth)
{
    if (depthWriteEnabled_ == enableDepth)
        return;

    glDepthMask(enableDepth ? GL_TRUE : GL_FALSE);
    depthWriteEnabled_ = enableDepth;
}

void GLState::setFaceCull(GLenum mode)
{
    if (currentFaceCull_ == mode)
        return;

    glCullFace(mode);
    currentFaceCull_ = mode;
}
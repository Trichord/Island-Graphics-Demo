#include "renderer/shader/graphics_shader.h"

#include <array>

GraphicsShader::GraphicsShader(ShaderType type, const std::filesystem::path &vertexPath,
                               const std::filesystem::path &fragmentPath)
{
    std::array<GLuint, 2> sourceIDs;

    sourceIDs[0] = createShaderID(GL_VERTEX_SHADER, vertexPath);
    sourceIDs[1] = createShaderID(GL_FRAGMENT_SHADER, fragmentPath);

    linkShader(sourceIDs, vertexPath.stem().string());
}

ShaderType GraphicsShader::getType() const
{
    return type_;
}
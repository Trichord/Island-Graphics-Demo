#include "renderer/shader/compute_shader.h"

#include <array>
#include <filesystem>
#include <glad/glad.h>
#include <string>
#include <vector>

ComputeShader::ComputeShader(const std::filesystem::path &path, const std::vector<std::string> &defines)
{
    std::array<GLuint, 1> sourceID;

    sourceID[0] = createShaderID(GL_COMPUTE_SHADER, path, defines);
    linkShader(sourceID, path.stem().string());
}

void ComputeShader::dispatch(std::uint32_t x, std::uint32_t y, std::uint32_t z)
{
    glDispatchCompute(static_cast<GLuint>(x), static_cast<GLuint>(y), static_cast<GLuint>(z));
}
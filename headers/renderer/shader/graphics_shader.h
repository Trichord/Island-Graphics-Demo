#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "renderer/shader/shader_base.h"

enum class ShaderType : std::uint8_t
{
    baseColor,
    pbr,
    reflectiveCubemap,
    vegetation,
    particles,
    ocean,
    skybox,
    water,
    trees,
};

/**
 * @brief Creates a linked vertex / fragment shader program.
 */
class GraphicsShader : public Shader
{
  public:
    GraphicsShader() = default;
    GraphicsShader(const std::filesystem::path &vertexPath, const std::filesystem::path &fragmentPath,
                   const std::vector<std::string> &defines = {});
    GraphicsShader(const GraphicsShader &) = delete;
    GraphicsShader &operator=(const GraphicsShader &) = delete;
    GraphicsShader(GraphicsShader &&other) = default;
    GraphicsShader &operator=(GraphicsShader &&other) = default;
};

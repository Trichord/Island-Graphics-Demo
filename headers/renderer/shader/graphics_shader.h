#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "renderer/shader/shader_base.h"

/* When a scene is passed to the renderer, these enums will be rendered in order.
 */
enum class ShaderType : std::uint8_t
{
    baseColor,
    pbr,
    explodingPBR,
    reflectiveCubemap,
    vegetation,
    particles,
    ocean,
    skybox,
    water,
    trees,
};

class GraphicsShader : public Shader
{
  public:
    GraphicsShader() = default;
    GraphicsShader(const std::filesystem::path &vertexPath, const std::filesystem::path &fragmentPath,
                   const std::vector<std::string> &defines = {});
    GraphicsShader(GraphicsShader &&other) = default;
    GraphicsShader &operator=(GraphicsShader &&other) = default;
};

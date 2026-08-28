#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "renderer/shader/shader_base.h"

/* When a scene is passed to the renderer, these enums will be rendered in order.
 */
enum class ShaderType
{
    baseColor,
    directionalPBR,
    reflective,
    trees,
    grass,
    particles,
    ocean,
    skybox,
    water, // Blended, after all opaque
    count
};

constexpr int shaderIndex(ShaderType type)
{
    return static_cast<int>(type);
}

class GraphicsShader : public Shader
{
  public:
    GraphicsShader() = default;
    GraphicsShader(const std::filesystem::path &vertexPath, const std::filesystem::path &fragmentPath,
                   const std::vector<std::string> &defines = {});
    GraphicsShader(GraphicsShader &&other) = default;
    GraphicsShader &operator=(GraphicsShader &&other) = default;

    ShaderType getType() const;
};

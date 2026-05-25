#pragma once

#include <filesystem>

#include "renderer/shader/shader_base.h"

/* When a scene is passed to the renderer, these enums will be rendered in order.
 */

enum class ShaderType
{
    directionalPBR,
    reflective,
    trees,
    grass,
    water,
    ocean,
    particles,
    skybox,
    shadowMap,
    shadowMapAlpha,
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
    GraphicsShader(ShaderType type, const std::filesystem::path &vertexPath, const std::filesystem::path &fragmentPath);
    GraphicsShader(GraphicsShader &&other) = default;
    GraphicsShader &operator=(GraphicsShader &&other) = default;

    ShaderType getType() const;

  private:
    ShaderType type_{ShaderType::directionalPBR};
};

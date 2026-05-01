#pragma once

#include "shader_base.h"

/* Order is important.When a scene is passed to the renderer, these enums will
 * be rendered in order (skybox is usually last) as an optimization.
 */

enum class ShaderType : std::size_t
{
    directionalPBR,
    reflective,
    trees,
    grass,
    water,
    ocean,
    skybox,
    shadowMap,
    shadowMapAlpha,
    count
};

constexpr std::size_t shaderIndex(ShaderType type)
{
    return static_cast<std::size_t>(type);
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

#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include "glad/glad.h"
#include "glm/glm.hpp"

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
    shadowMapAlphaTested,
    count
};

constexpr std::size_t shaderIndex(ShaderType type)
{
    return static_cast<std::size_t>(type);
}

/* Owns program ID, no copying */
class Shader
{
  public:
    Shader() = default;
    Shader(ShaderType type, const std::filesystem::path &vertexPath, const std::filesystem::path &fragmentPath);
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;
    Shader(Shader &&other) noexcept;
    Shader &operator=(Shader &&other) noexcept;
    inline explicit operator bool() const noexcept
    {
        return id_ != 0;
    }
    ~Shader();

    void use();

    GLuint getID() const;
    ShaderType getType() const;

    void setBool(const std::string &name, bool value);
    void setIntU(const std::string &name, unsigned int value);
    void setInt(const std::string &name, int value);
    void setFloat(const std::string &name, float value);
    void setVec2(const std::string &name, glm::vec2 value);
    void setVec3(const std::string &name, glm::vec3 value);
    void setVec4(const std::string &name, glm::vec4 value);
    void setMat3(const std::string &name, const glm::mat3 &value);
    void setMat4(const std::string &name, const glm::mat4 &value);

  private:
    GLuint id_{0};
    ShaderType type_{ShaderType::directionalPBR};
    std::unordered_map<std::string, GLint> uniformLocations_;

    GLint getCachedUniform(const std::string &name);
    GLuint createShaderID(GLenum type, const std::filesystem::path &path);
};

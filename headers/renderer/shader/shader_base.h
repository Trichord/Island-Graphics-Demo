#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <unordered_map>

#include "glad/glad.h"
#include "glm/glm.hpp"

class Shader
{
  public:
    inline explicit operator bool() const noexcept
    {
        return id_ != 0;
    }

    void use();
    GLuint getID() const;

    void setBool(const std::string &name, bool value);
    void setIntU(const std::string &name, unsigned int value);
    void setInt(const std::string &name, int value);
    void setFloat(const std::string &name, float value);
    void setVec2(const std::string &name, glm::vec2 value);
    void setVec3(const std::string &name, glm::vec3 value);
    void setVec4(const std::string &name, glm::vec4 value);
    void setMat3(const std::string &name, const glm::mat3 &value);
    void setMat4(const std::string &name, const glm::mat4 &value);

  protected:
    Shader() = default;
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;
    Shader(Shader &&other) noexcept;
    Shader &operator=(Shader &&other) noexcept;
    ~Shader();

    GLuint id_{0};
    std::unordered_map<std::string, GLint> uniformLocations_;

    GLint getCachedUniform(const std::string &name);

    GLuint createShaderID(GLenum type, const std::filesystem::path &path);
    void linkShader(std::span<GLuint> sourceIDs, const std::string_view path);
};

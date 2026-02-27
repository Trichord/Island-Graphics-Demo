#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "glad/glad.h"
#include "glm/glm.hpp"

/* Owns program ID, no copying */

class Shader
{
  public:
    // Shader program ID
    GLuint ID{0};

    Shader() = default;
    Shader(const std::filesystem::path &vertexPath, const std::filesystem::path &fragmentPath);
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;
    Shader(Shader &&other) noexcept;
    Shader &operator=(Shader &&other) noexcept;
    inline explicit operator bool() const noexcept
    {
        return ID != 0;
    }
    ~Shader();

    void use();

    void setBool(const std::string &name, bool value);
    void setIntU(const std::string &name, unsigned int value);
    void setInt(const std::string &name, int value);
    void setFloat(const std::string &name, float value);
    void setVec2(const std::string &name, glm::vec2 value);
    void setVec3(const std::string &name, glm::vec3 value);
    void setVec4(const std::string &name, glm::vec4 value);
    void setMat3(const std::string &name, const glm::mat3 &value);
    void setMat4(const std::string &name, const glm::mat4 &value);
    void setUniformBlock(const std::string &name, unsigned int index);

  private:
    std::unordered_map<std::string, GLint> uniformLocations_;
    std::unordered_map<std::string, GLuint> blockLocations_;

    GLuint getCachedBlock(const std::string &name);
    GLint getCachedUniform(const std::string &name);
    GLuint createShaderID(GLenum type, const std::filesystem::path &path);
};

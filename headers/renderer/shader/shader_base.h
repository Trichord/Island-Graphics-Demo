// SPDX-License-Identifier: CC-BY-NC-4.0
//
// Derived from LearnOpenGL (https://learnopengl.com) by Joey de Vries.
// Original code licensed under CC BY-NC 4.0: https://creativecommons.org/licenses/by-nc/4.0/
// Modified by James Midtdal: Switched to DSA, added cached lookups and define injection system.
//
// This file is not covered by the project's MIT License.

#pragma once

#include <filesystem>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Shader
{
  public:
    inline explicit operator bool() const noexcept
    {
        return id_ != 0;
    }

    void use() const;
    GLuint getID() const;

    void setBool(const std::string &name, bool value) const;
    void setIntU(const std::string &name, unsigned int value) const;
    void setInt(const std::string &name, int value) const;
    void setFloat(const std::string &name, float value) const;
    void setVec2(const std::string &name, const glm::vec2 &value) const;
    void setVec3(const std::string &name, const glm::vec3 &value) const;
    void setVec4(const std::string &name, const glm::vec4 &value) const;
    void setIVec2(const std::string &name, const glm::ivec2 &value) const;
    void setIVec3(const std::string &name, const glm::ivec3 &value) const;
    void setIVec4(const std::string &name, const glm::ivec4 &value) const;
    void setMat3(const std::string &name, const glm::mat3 &value) const;
    void setMat4(const std::string &name, const glm::mat4 &value) const;

  protected:
    static constexpr std::string_view shaderBasePath_{"./shaders/"};

    Shader() = default;
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;
    Shader(Shader &&other) noexcept;
    Shader &operator=(Shader &&other) noexcept;
    ~Shader();

    GLuint id_{0};
    mutable std::unordered_map<std::string, GLint> uniformLocations_;

    GLint getCachedUniform(const std::string &name) const;

    GLuint createShaderID(GLenum type, const std::filesystem::path &path, const std::vector<std::string> &defines = {});
    void linkShader(std::span<GLuint> sourceIDs, const std::string_view path);
    std::string preProcess(const std::string &shaderString, const std::vector<std::string> &defines,
                           std::unordered_set<std::string> &seen, std::size_t lineNum);
    std::string loadFile(const std::filesystem::path &path);
};

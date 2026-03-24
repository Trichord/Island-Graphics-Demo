#include "shader.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include "glad/glad.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

Shader::Shader(const std::filesystem::path &vertexPath, const std::filesystem::path &fragmentPath)
{
    GLuint vertexShaderID{0};
    GLuint fragmentShaderID{0};

    vertexShaderID = createShaderID(GL_VERTEX_SHADER, vertexPath);
    fragmentShaderID = createShaderID(GL_FRAGMENT_SHADER, fragmentPath);

    id_ = glCreateProgram();

    glAttachShader(id_, vertexShaderID);
    glAttachShader(id_, fragmentShaderID);
    glLinkProgram(id_);

    int success{0};
    glGetProgramiv(id_, GL_LINK_STATUS, &success);

    if (!success)
    {
        std::string infoLog;
        GLint logLength;
        glGetProgramiv(id_, GL_INFO_LOG_LENGTH, &logLength);
        infoLog.resize(logLength);

        glGetProgramInfoLog(id_, logLength, NULL, infoLog.data());
        std::cerr << "ERROR: Shader Program - Linking Failed" << std::endl << infoLog.data() << std::endl;

        glDeleteShader(vertexShaderID);
        glDeleteShader(fragmentShaderID);

        throw std::runtime_error("Shader program linking failed");
    }

    glDeleteShader(vertexShaderID);
    glDeleteShader(fragmentShaderID);
}

Shader::Shader(Shader &&other) noexcept
{
    id_ = other.id_;
    uniformLocations_ = std::move(other.uniformLocations_);
    other.id_ = 0;
};

Shader &Shader::operator=(Shader &&other) noexcept
{
    if (this != &other)
    {
        if (id_)
            glDeleteProgram(id_);

        id_ = other.id_;
        uniformLocations_ = std::move(other.uniformLocations_);
        other.id_ = 0;
    }

    return *this;
};

Shader::~Shader()
{
    if (id_)
        glDeleteProgram(id_);
}

void Shader::use()
{
    glUseProgram(id_);
}

GLuint Shader::getID() const
{
    return id_;
}

void Shader::setBool(const std::string &name, bool value)
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniform1i(id_, location, value);
}

void Shader::setIntU(const std::string &name, unsigned int value)
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniform1ui(id_, location, value);
}

void Shader::setInt(const std::string &name, int value)
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniform1i(id_, location, value);
}

void Shader::setFloat(const std::string &name, float value)
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniform1f(id_, location, value);
}

void Shader::setMat3(const std::string &name, const glm::mat3 &value)
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniformMatrix3fv(id_, location, 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setMat4(const std::string &name, const glm::mat4 &value)
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniformMatrix4fv(id_, location, 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setVec2(const std::string &name, glm::vec2 value)
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniform2fv(id_, location, 1, glm::value_ptr(value));
}

void Shader::setVec3(const std::string &name, glm::vec3 value)
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniform3fv(id_, location, 1, glm::value_ptr(value));
}

void Shader::setVec4(const std::string &name, glm::vec4 value)
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniform4fv(id_, location, 1, glm::value_ptr(value));
}

GLint Shader::getCachedUniform(const std::string &name)
{
    auto [iter, inserted] = uniformLocations_.try_emplace(name, -1);

    if (inserted)
        iter->second = glGetUniformLocation(id_, name.c_str());

    return iter->second;
}

GLuint Shader::createShaderID(GLenum type, const std::filesystem::path &path)
{
    std::string fileContents;

    std::ifstream fileStream;
    fileStream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try
    {
        fileStream.open(path);
        fileContents = std::string{std::istreambuf_iterator<char>{fileStream}, std::istreambuf_iterator<char>{}};
    }

    catch (const std::ifstream::failure &e)
    {
        std::cerr << "ERROR: shader file " << path << " read unsuccessful" << std::endl << e.what() << std::endl;
        throw;
    }

    GLuint shaderID{0};
    GLint success{0};
    const char *shaderCode{fileContents.c_str()};

    shaderID = glCreateShader(type);
    glShaderSource(shaderID, 1, &shaderCode, nullptr);
    glCompileShader(shaderID);
    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        std::string infoLog;
        GLint logLength;
        glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &logLength);
        infoLog.resize(logLength);

        glGetShaderInfoLog(shaderID, logLength, nullptr, infoLog.data());

        std::string shaderTypeString;

        switch (type)
        {
        case GL_VERTEX_SHADER:
            shaderTypeString = "GL_VERTEX_SHADER";
            break;
        case GL_FRAGMENT_SHADER:
            shaderTypeString = "GL_FRAGMENT_SHADER";
            break;
        default:
            shaderTypeString = "UNKNOWN_SHADER_TYPE";
        }

        std::cerr << "ERROR: shader type " << shaderTypeString << " at " << path.filename() << " compilation failed"
                  << std::endl
                  << infoLog.data() << std::endl;
        glDeleteShader(shaderID);

        throw std::runtime_error("Shader compilation failed");
    }

    return shaderID;
}
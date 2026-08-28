#include "renderer/shader/shader_base.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ios>
#include <iostream>
#include <iterator>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

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

void Shader::use() const
{
    glUseProgram(id_);
}

GLuint Shader::getID() const
{
    return id_;
}

void Shader::setBool(const std::string &name, bool value) const
{

    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniform1i(id_, location, value);
}

void Shader::setIntU(const std::string &name, unsigned int value) const
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniform1ui(id_, location, value);
}

void Shader::setInt(const std::string &name, int value) const
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniform1i(id_, location, value);
}

void Shader::setFloat(const std::string &name, float value) const
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniform1f(id_, location, value);
}

void Shader::setMat3(const std::string &name, const glm::mat3 &value) const
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniformMatrix3fv(id_, location, 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setMat4(const std::string &name, const glm::mat4 &value) const
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniformMatrix4fv(id_, location, 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setVec2(const std::string &name, const glm::vec2 &value) const
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniform2fv(id_, location, 1, glm::value_ptr(value));
}

void Shader::setVec3(const std::string &name, const glm::vec3 &value) const
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniform3fv(id_, location, 1, glm::value_ptr(value));
}

void Shader::setVec4(const std::string &name, const glm::vec4 &value) const
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniform4fv(id_, location, 1, glm::value_ptr(value));
}

void Shader::setIVec2(const std::string &name, const glm::ivec2 &value) const
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniform2iv(id_, location, 1, glm::value_ptr(value));
}

void Shader::setIVec3(const std::string &name, const glm::ivec3 &value) const
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniform3iv(id_, location, 1, glm::value_ptr(value));
}

void Shader::setIVec4(const std::string &name, const glm::ivec4 &value) const
{
    GLint location{getCachedUniform(name)};

    if (location != -1)
        glProgramUniform4iv(id_, location, 1, glm::value_ptr(value));
}

GLint Shader::getCachedUniform(const std::string &name) const
{
    auto [iter, inserted] = uniformLocations_.try_emplace(name, -1);

    if (inserted)
        iter->second = glGetUniformLocation(id_, name.c_str());

    return iter->second;
}

GLuint Shader::createShaderID(GLenum type, const std::filesystem::path &path, const std::vector<std::string> &defines)
{
    std::string fileContents{loadFile(path)};

    std::unordered_set<std::string> seen;
    std::string codeString{preProcess(fileContents, defines, seen, 0)};

    GLuint shaderID{0};
    GLint success{0};
    const char *shaderCode{codeString.c_str()};

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
        case GL_COMPUTE_SHADER:
            shaderTypeString = "GL_COMPUTE_SHADER";
            break;
        default:
            shaderTypeString = "UNKNOWN_SHADER_TYPE";
        }

        std::cerr << "ERROR: " << shaderTypeString << " compilation from " << path.filename() << " failed" << std::endl
                  << infoLog.data() << std::endl;
        glDeleteShader(shaderID);

        throw std::runtime_error("Shader compilation failed");
    }

    return shaderID;
}

std::string Shader::loadFile(const std::filesystem::path &path)
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

    return fileContents;
}

std::string Shader::preProcess(const std::string &shaderString, const std::vector<std::string> &defines,
                               std::unordered_set<std::string> &seen, std::size_t lineNum)
{
    std::istringstream stream{shaderString};
    std::string result;

    // Rough starting point to avoid initial allocations
    result.reserve(shaderString.size());

    std::string line;

    bool inBlockComment{false};

    while (std::getline(stream, line))
    {
        ++lineNum;

        // Let #version come first
        if (lineNum == 2)
            for (const std::string &define : defines)
                if (!define.empty())
                    result += std::string{"#define " + define + '\n'};

        // Find the #include
        std::size_t found{line.find("#include")};

        // Detect block comments
        std::size_t blockStart{line.find("/*")};
        std::size_t blockEnd{line.find("*/")};

        if (blockStart != std::string::npos)
            inBlockComment = true;

        if (blockEnd != std::string::npos)
        {
            inBlockComment = false;
            continue;
        }

        if (inBlockComment)
            continue;

        if (found != std::string::npos)
        {
            // Single line block quote
            if (blockStart != std::string::npos && blockEnd != std::string::npos && found > blockStart &&
                found < blockEnd)
                continue;

            std::size_t left{line.find('"')};
            std::size_t right{line.rfind('"')};

            if (left == std::string::npos)
                continue;

            std::size_t comment{line.find("//")};

            if (comment < left)
                continue;

            std::string includeName{line.substr(left + 1, right - left - 1)};

            auto [iter, inserted] = seen.emplace(includeName);

            // Try to avoid circular dependencies
            if (!inserted)
                continue;

            std::filesystem::path path{std::string{shaderBasePath_} + includeName};

            std::string fileContents{loadFile(path)};

            result += preProcess(fileContents, defines, seen, lineNum);
            continue;
        }

        result.append(line).append(1, '\n');
    }

    return result;
}

void Shader::linkShader(std::span<GLuint> sourceIDs, const std::string_view path)
{
    id_ = glCreateProgram();

    for (GLuint sourceID : sourceIDs)
        glAttachShader(id_, sourceID);

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
        std::cerr << "ERROR: Shader program link error: " << path << std::endl << infoLog.data() << std::endl;

        for (GLuint sourceID : sourceIDs)
            glDeleteShader(sourceID);

        throw std::runtime_error("Shader program linking failed");
    }

    for (GLuint sourceID : sourceIDs)
        glDeleteShader(sourceID);
}

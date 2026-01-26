#include "shader.h"

#include <array>
#include <fstream>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

#include "glad/glad.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

Shader::Shader(const std::filesystem::path& vertexPath,
               const std::filesystem::path& fragmentPath,
               const std::filesystem::path& geometryPath) {
  GLuint vertexShaderID{0};
  GLuint fragmentShaderID{0};
  GLuint geometryShaderID{0};

  vertexShaderID = createShaderID(GL_VERTEX_SHADER, vertexPath);
  fragmentShaderID = createShaderID(GL_FRAGMENT_SHADER, fragmentPath);
  geometryShaderID = createShaderID(GL_GEOMETRY_SHADER, geometryPath);

  if (!(vertexShaderID && fragmentShaderID)) return;

  ID = glCreateProgram();

  glAttachShader(ID, vertexShaderID);
  glAttachShader(ID, fragmentShaderID);

  if (geometryShaderID) glAttachShader(ID, geometryShaderID);

  glLinkProgram(ID);

  int success{0};
  glGetProgramiv(ID, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(ID, 512, NULL, infoLog.data());
    std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED" << std::endl
              << infoLog.data() << std::endl;
    return;
  }

  glDeleteShader(vertexShaderID);
  glDeleteShader(fragmentShaderID);

  if (geometryShaderID) glDeleteShader(geometryShaderID);
}

Shader::Shader(Shader&& other) noexcept {
  ID = other.ID;
  uniformLocations = std::move(other.uniformLocations);
  infoLog = std::move(other.infoLog);
  other.ID = 0;
};

Shader& Shader::operator=(Shader&& other) noexcept {
  if (this != &other) {
    if (ID) glDeleteProgram(ID);

    ID = other.ID;
    uniformLocations = std::move(other.uniformLocations);
    infoLog = std::move(other.infoLog);
    other.ID = 0;
  }

  return *this;
};

Shader::~Shader() {
  if (ID) glDeleteProgram(ID);
}

void Shader::use() { glUseProgram(ID); }

void Shader::setBool(const std::string& name, bool value) {
  GLint location{getCachedLocation(name)};

  if (location != -1) glProgramUniform1i(ID, location, value);
}

void Shader::setIntU(const std::string& name, unsigned int value) {
  GLint location{getCachedLocation(name)};

  if (location != -1) glProgramUniform1ui(ID, location, value);
}

void Shader::setInt(const std::string& name, int value) {
  GLint location{getCachedLocation(name)};

  if (location != -1) glProgramUniform1i(ID, location, value);
}

void Shader::setFloat(const std::string& name, float value) {
  GLint location{getCachedLocation(name)};

  if (location != -1) glProgramUniform1f(ID, location, value);
}

void Shader::setMat3(const std::string& name, const glm::mat3& value) {
  GLint location{getCachedLocation(name)};

  if (location != -1)
    glProgramUniformMatrix3fv(ID, location, 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) {
  GLint location{getCachedLocation(name)};

  if (location != -1)
    glProgramUniformMatrix4fv(ID, location, 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::setVec2(const std::string& name, glm::vec2 value) {
  GLint location{getCachedLocation(name)};

  if (location != -1)
    glProgramUniform2fv(ID, location, 1, glm::value_ptr(value));
}

void Shader::setVec2(const std::string& name, float x, float y) {
  GLint location{getCachedLocation(name)};

  if (location != -1) glProgramUniform2f(ID, location, x, y);
}

void Shader::setVec3(const std::string& name, glm::vec3 value) {
  GLint location{getCachedLocation(name)};

  if (location != -1)
    glProgramUniform3fv(ID, location, 1, glm::value_ptr(value));
}

void Shader::setVec3(const std::string& name, float x, float y, float z) {
  GLint location{getCachedLocation(name)};

  if (location != -1) glProgramUniform3f(ID, location, x, y, z);
}

void Shader::setVec4(const std::string& name, glm::vec4 value) {
  GLint location{getCachedLocation(name)};

  if (location != -1)
    glProgramUniform4fv(ID, location, 1, glm::value_ptr(value));
}

void Shader::setVec4(const std::string& name, float x, float y, float z,
                     float w) {
  GLint location{getCachedLocation(name)};

  if (location != -1) glProgramUniform4f(ID, location, x, y, z, w);
}

void Shader::setUniformBlock(const std::string& name, unsigned int index) {
  auto iter = uniformLocations.find(name);

  if (iter != uniformLocations.end() && iter->second != -1) {
    glUniformBlockBinding(ID, iter->second, index);
    return;
  }

  GLuint blockID{glGetUniformBlockIndex(ID, name.c_str())};
  uniformLocations.emplace(name, blockID);

  if (blockID != -1) glUniformBlockBinding(ID, blockID, index);
}

GLint Shader::getCachedLocation(const std::string& name) {
  auto iter = uniformLocations.find(name);
  if (iter != uniformLocations.end()) return iter->second;

  GLint location{glGetUniformLocation(ID, name.c_str())};
  uniformLocations.emplace(name, location);

  return location;
}

GLuint Shader::createShaderID(GLenum type, const std::filesystem::path& path) {
  if (path.empty()) return 0;

  std::string fileContents;

  try {
    std::ifstream fileStream{path};
    fileStream.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    fileContents = std::string{std::istreambuf_iterator<char>{fileStream},
                               std::istreambuf_iterator<char>{}};
  }

  catch (const std::ifstream::failure& e) {
    std::cerr << "ERROR: shader file not successfully read!" << std::endl
              << e.what() << std::endl;
    return 0;
  }

  GLuint shaderID{0};
  int success{0};
  const char* shaderCode{fileContents.c_str()};

  shaderID = glCreateShader(type);
  glShaderSource(shaderID, 1, &shaderCode, nullptr);
  glCompileShader(shaderID);
  glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);

  if (!success) {
    glGetShaderInfoLog(shaderID, 512, nullptr, infoLog.data());

    std::string shaderTypeString;

    switch (type) {
      case GL_VERTEX_SHADER:
        shaderTypeString = "GL_VERTEX_SHADER";
        break;
      case GL_FRAGMENT_SHADER:
        shaderTypeString = "GL_FRAGMENT_SHADER";
        break;
      case GL_GEOMETRY_SHADER:
        shaderTypeString = "GL_GEOMETRY_SHADER";
        break;
      default:
        shaderTypeString = "UNKNOWN_SHADER_TYPE";
    }

    std::cerr << "ERROR: shader type " << shaderTypeString << " at "
              << path.filename() << " compilation failed" << std::endl
              << infoLog.data() << std::endl;
    glDeleteShader(shaderID);
    return 0;
  }

  return shaderID;
}
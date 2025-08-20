#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <array>

//Owns program ID, no copying
class Shader {
public:
	//Shader program ID
	GLuint ID = 0;

	Shader(const char* vertexPath, const char* fragmentPath, const char* geometryPath = NULL) {
		GLuint vertexShaderID, fragmentShaderID, geometryShaderID;

		vertexShaderID = createShaderID(GL_VERTEX_SHADER, vertexPath);
		fragmentShaderID = createShaderID(GL_FRAGMENT_SHADER, fragmentPath);
		geometryShaderID = createShaderID(GL_GEOMETRY_SHADER, geometryPath);

		if (!(vertexShaderID && fragmentShaderID))
			return;

		ID = glCreateProgram();

		glAttachShader(ID, vertexShaderID);
		glAttachShader(ID, fragmentShaderID);
		
		if (geometryShaderID)
			glAttachShader(ID, geometryShaderID);

		glLinkProgram(ID);

		int success;
		glGetProgramiv(ID, GL_LINK_STATUS, &success);
		if (!success) {
			glGetProgramInfoLog(ID, 512, NULL, infoLog.data());
			std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED" << std::endl << infoLog.data() << std::endl;
			return;
		}

		glDeleteShader(vertexShaderID);
		glDeleteShader(fragmentShaderID);

		if (geometryShaderID)
			glDeleteShader(geometryShaderID);
	}

	Shader(const Shader&) = delete;
	Shader& operator=(const Shader&) = delete;

	Shader(Shader&& other) noexcept {
		ID = other.ID;
		uniformLocations = std::move(other.uniformLocations);
		infoLog = std::move(other.infoLog);
		other.ID = 0;
	};

	Shader& operator=(Shader&& other) noexcept {
		if (this != &other) {
			if (ID)
				glDeleteProgram(ID);

			ID = other.ID;
			uniformLocations = std::move(other.uniformLocations);
			infoLog = std::move(other.infoLog);
			other.ID = 0;
		}

		return *this;
	};

	explicit operator bool() const noexcept { return ID != 0; }

	~Shader() {
		if(ID)
			glDeleteProgram(ID);
	}

	void use() {
		glUseProgram(ID);
	}

	void setBool(const std::string &name, bool value) {
		GLint location = getCachedLocation(name);

		if (location != -1)
			glUniform1i(location, value);
	}

	void setIntU(const std::string &name, unsigned int value) {
		GLint location = getCachedLocation(name);

		if (location != -1)
			glUniform1ui(location, value);
	}

	void setInt(const std::string &name, int value)  {
		GLint location = getCachedLocation(name);

		if (location != -1)
			glUniform1i(location, value);
	}

	void setFloat(const std::string &name, float value)  {
		GLint location = getCachedLocation(name);

		if (location != -1)
			glUniform1f(location, value);
	}

	void setMat3(const std::string &name, const glm::mat3 &value)  {
		GLint location = getCachedLocation(name);

		if (location != -1)
			glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(value));
	}

	void setMat4(const std::string &name, const glm::mat4 &value)  {
		GLint location = getCachedLocation(name);

		if (location != -1)
			glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
	}

	void setVec2(const std::string &name, glm::vec2 value)  {
		GLint location = getCachedLocation(name);

		if (location != -1)
			glUniform2fv(location, 1, glm::value_ptr(value));
	}

	void setVec2(const std::string &name, float x, float y)  {
		GLint location = getCachedLocation(name);

		if (location != -1)
			glUniform2f(location, x, y);
	}

	void setVec3(const std::string &name, glm::vec3 value)  {
		GLint location = getCachedLocation(name);

		if (location != -1)
			glUniform3fv(location, 1, glm::value_ptr(value));
	}

	void setVec3(const std::string &name, float x, float y, float z)  {
		GLint location = getCachedLocation(name);

		if (location != -1)
			glUniform3f(location, x, y, z);
	}

	void setVec4(const std::string &name, glm::vec4 value)  {
		GLint location = getCachedLocation(name);

		if (location != -1)
			glUniform4fv(location, 1, glm::value_ptr(value));
	}

	void setVec4(const std::string &name, float x, float y, float z, float w)  {
		GLint location = getCachedLocation(name);

		if (location != -1)
			glUniform4f(location, x, y, z, w);
	}

	void setUniformBlock(const std::string& name, unsigned int index) {
		auto iter = uniformLocations.find(name);

		if (iter != uniformLocations.end()) {
			glUniformBlockBinding(ID, iter->second, index);
			return;
		}

		GLint blockID = glGetUniformBlockIndex(ID, name.c_str());
		
		if (blockID != -1) {
			uniformLocations.emplace(name, blockID);
			glUniformBlockBinding(ID, blockID, index);
		}
		else
			std::cerr << "Unknown uniform Block: " << name << std::endl;	
	}

private:
	std::array<char, 512> infoLog{};
	std::unordered_map<std::string, GLint> uniformLocations;

	GLint getCachedLocation(const std::string &name) {
		auto iter = uniformLocations.find(name);

		if (iter != uniformLocations.end())
			return iter->second;

		GLint location = glGetUniformLocation(ID, name.c_str());

		if (location != -1)
			uniformLocations.emplace(name, location);
		//else
			//std::cout << "Unknown uniform: " << name << std::endl;

		return location;
	}

	GLuint createShaderID(GLenum type, const char* path) {

		if (!path)
			return 0;

		std::string code;
		std::ifstream shaderFile;

		//Ensure ifstream objects can throw exceptions
		shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

		try {
			shaderFile.open(path);

			std::stringstream shaderStream;

			//Read file buffer contents into stream
			shaderStream << shaderFile.rdbuf();
			shaderFile.close();

			//convert stream to string
			code = shaderStream.str();
		}

		catch (std::ifstream::failure &e) {
			std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ::" << e.what() << std::endl;
			return 0;
		}

		unsigned int shaderCodeID;
		int success;
		const char* shaderCode = code.c_str();

		shaderCodeID = glCreateShader(type);
		glShaderSource(shaderCodeID, 1, &shaderCode, NULL);
		glCompileShader(shaderCodeID);

		glGetShaderiv(shaderCodeID, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(shaderCodeID, 512, NULL, infoLog.data());
			std::cerr << "ERROR::SHADER_TYPE_" << type << "::COMPILATION_FAILED" << std::endl << infoLog.data() << std::endl;
			glDeleteShader(shaderCodeID);
			return 0;
		}

		return shaderCodeID;
	}
};

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

class Shader {
public:
	//Shader program ID
	unsigned int ID;

	Shader(const char* vertexPath, const char* fragmentPath) {

		/*******************
		Read code from files
		********************/

		std::string vertexCode, fragmentCode;
		std::ifstream vShaderFile, fShaderFile;

		//Ensure ifstream objects can throw exceptions
		vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

		try {
			//open the files
			vShaderFile.open(vertexPath);
			fShaderFile.open(fragmentPath);
			std::stringstream vShaderStream, fShaderStream;
			
			//Read file buffer contents into stream
			vShaderStream << vShaderFile.rdbuf();
			fShaderStream << fShaderFile.rdbuf();

			vShaderFile.close();
			fShaderFile.close();

			//convert stream to string
			vertexCode = vShaderStream.str();
			fragmentCode = fShaderStream.str();
		}

		catch (std::ifstream::failure e) {
			std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ" << std::endl;
		}

		const char* vShaderCode = vertexCode.c_str();
		const char* fShaderCode = fragmentCode.c_str();

		/*************************
		Compile the shader program
		**************************/

		unsigned int vertex, fragment;
		int success;
		char infoLog[512];

		//Vertex shader
		vertex = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertex, 1, &vShaderCode, NULL);
		glCompileShader(vertex);

	
		glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(vertex, 512, NULL, infoLog);
			std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED" << std::endl << infoLog << std::endl;
		}

		//Fragment Shader
		fragment = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment, 1, &fShaderCode, NULL);
		glCompileShader(fragment);

		glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(fragment, 512, NULL, infoLog);
			std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED" << std::endl << infoLog << std::endl;
		}

		//Shader program
		ID = glCreateProgram();
		glAttachShader(ID, vertex);
		glAttachShader(ID, fragment);
		glLinkProgram(ID);

		glGetProgramiv(ID, GL_LINK_STATUS, &success);
		if (!success) {
			glGetProgramInfoLog(ID, 512, NULL, infoLog);
			std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED" << std::endl << infoLog << std::endl;
		}

		glDeleteShader(vertex);
		glDeleteShader(fragment);
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
			std::cout << "Unknown uniform " << name << std::endl;	
	}

private:
	std::unordered_map<std::string, GLint> uniformLocations;

	GLint getCachedLocation(const std::string &name) {
		auto iter = uniformLocations.find(name);

		if (iter != uniformLocations.end())
			return iter->second;

		GLint location = glGetUniformLocation(ID, name.c_str());

		if (location != -1)
			uniformLocations.emplace(name, location);
		else
			std::cout << "Unknown uniform: " << name << std::endl;

		return location;
	}
};

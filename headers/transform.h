#pragma once

#include <glm/glm.hpp>

class Transform {
public:


private:
	glm::vec3 scale;
	glm::vec3 position;
	glm::mat3 normalMatrix;
	glm::mat3 normalMVMatrix;
	glm::mat4 modelMatrix = glm::mat4(1.0f);
};
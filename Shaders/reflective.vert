#version 430 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNorm;

layout (std140) uniform matrices {
	mat4 viewMatrix;
	mat4 projectionViewMatrix;
};

uniform mat4 modelMatrix;
uniform mat3 normalMatrix;

out vec3 normal;
out vec3 fragPos;
out vec3 position;

void main() {
	normal = normalMatrix * aNorm;
	position = vec3(modelMatrix * vec4(aPos, 1.0f));
	gl_Position = projectionViewMatrix * vec4(position, 1.0f);
}
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNorm;

layout (std140) uniform matrices {
	mat4 viewMatrix;
	mat4 projectionViewMatrix;
};

uniform mat3 normalMatrix;
uniform mat4 modelMatrix;

out vec3 normal;
out vec3 fragPos;

void main() {
	normal = normalMatrix * aNorm;
	fragPos = vec3(viewMatrix * modelMatrix * vec4(aPos, 1.0f));
	gl_Position = projectionViewMatrix * modelMatrix * vec4(aPos, 1.0f);
}
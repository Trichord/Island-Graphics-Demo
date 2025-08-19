#version 330 core

layout (location = 0) in vec3 aPos;

layout (std140) uniform matrices {
	mat4 viewMatrix;
	mat4 projectionMatrix;
};

uniform mat4 modelMatrix;

void main() {
	gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.0f);
}
#version 460 core

layout (location = 0) in vec3 aPos;

uniform mat4 lightSpaceMatrix;
uniform mat4 modelMatrix;

void main() {
	gl_Position = lightSpaceMatrix * modelMatrix * vec4(aPos, 1.0f);
}
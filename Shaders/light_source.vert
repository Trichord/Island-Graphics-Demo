#version 460 core

layout (location = 0) in vec3 aPos;

layout (std140) uniform matrices {
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionViewMatrix;
	layout (offset = 128) vec4 cameraPosition;
};

uniform mat4 modelMatrix;

void main() {
	gl_Position = projectionViewMatrix * modelMatrix * vec4(aPos, 1.0f);
}
#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;

out vec3 texCoords;

void main() {
	texCoords = aPos;
	vec4 pos = projectionMatrix * viewMatrix * vec4(aPos, 1.0f);
	gl_Position = pos.xyww;
}
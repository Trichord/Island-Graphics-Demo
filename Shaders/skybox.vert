#version 330 core

layout (location = 0) in vec3 aPos;

layout (std140) uniform matrices {
	mat4 viewMatrix;
	mat4 projectionMatrix;
};

uniform mat4 viewNoTrans;

out vec3 texCoords;

void main() {
	texCoords = aPos;
	vec4 pos = projectionMatrix * viewNoTrans * vec4(aPos, 1.0f);
	gl_Position = pos.xyww;
}
#version 460 core

layout (location = 0) in vec3 aPos;

layout (std140) uniform matrices {
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
};

out vec3 texCoords;

void main() {
	texCoords = aPos;

	mat4 viewNoTrans = mat4(mat3(viewMatrix));

	vec4 pos = projectionMatrix * viewNoTrans * vec4(aPos, 1.0f);
	gl_Position = pos.xyww;
}
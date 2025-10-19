#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;
layout (location = 2) in mat4 aTransform;
layout (location = 6) in mat3 aNormalMat;

layout (std140) uniform matrices {
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionViewMatrix;
	layout (offset = 128) vec4 cameraPosition;
};

out vec2 texCoords;
out vec3 normal;

void main() {
	texCoords = aTexCoords; 
	normal = aNormalMat * vec3(0.0, 1.0, 0.0);

    gl_Position = projectionViewMatrix * aTransform * vec4(aPos, 1.0);
}
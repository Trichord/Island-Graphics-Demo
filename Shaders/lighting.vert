#version 440 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 atexCoords;

layout (std140) uniform matrices {
	mat4 viewMatrix;
	mat4 projectionMatrix;
};

uniform mat4 modelMatrix;
uniform mat3 normalMatrix;
uniform vec3 uLightPosition;

out vec3 normalVector;
out vec3 fragPosition;
out vec2 texCoords;

void main() {
	normalVector = normalMatrix * aNormal;
	fragPosition = vec3(modelMatrix * vec4(aPos, 1.0f));
	texCoords = atexCoords;
	gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.0f);
}
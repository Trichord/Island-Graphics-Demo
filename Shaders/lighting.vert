#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

uniform vec3 uLightPosition;

uniform mat4 modelMatrix;
uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform mat3 normalMatrix;

out vec3 vNormalVector;
out vec3 vFragPosition;
out vec2 vTexCoords;

void main() {
	vNormalVector = normalMatrix * aNormal;
	vFragPosition = vec3(modelMatrix * vec4(aPos, 1.0f));
	vTexCoords = aTexCoords;
	gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.0f);
}
#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uvCoords;

layout (std140) uniform matrices {
	mat4 viewMatrix;
	mat4 projectionMatrix;
};

out VS_OUT {
	vec3 fragPos;
	vec3 normalVector;
	vec2 texCoords;
} vs_out;

uniform mat4 modelMatrix;
uniform mat3 normalMatrix;

void main () {
	vs_out.fragPos = vec3(modelMatrix * vec4(pos, 1.0));
	vs_out.normalVector = normalMatrix * normal;
	vs_out.texCoords = uvCoords;
	gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(pos, 1.0);
}
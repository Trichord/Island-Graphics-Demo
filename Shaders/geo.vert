#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 uvCoords;

layout (std140) uniform matrices {
	mat4 viewMatrix;
	mat4 projectionMatrix;
};

out VS_OUT {
	vec2 texCoords;
} vs_out;

uniform mat4 modelMatrix;

void main() {
	gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.0f);
	vs_out.texCoords = uvCoords;
}

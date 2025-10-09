#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 atexCoords;

layout (std140) uniform matrices {
	mat4 viewMatrix;
	mat4 projectionMatrix;
};

uniform mat4 model;

out vec2 texCoords;

void main() {
    texCoords = atexCoords;    
    gl_Position = projectionMatrix * viewMatrix * model * vec4(aPos, 1.0);
}
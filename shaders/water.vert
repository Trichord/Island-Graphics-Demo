#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 3) in vec2 aTexCoords;

layout (std140) uniform matrices 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionViewMatrix;
	layout (offset = 128) vec4 cameraPosition;
};

out VS_OUT 
{
	vec2 texCoords;
	vec3 fragPos;
	vec4 clipSpace;
} vs_out;

uniform mat4 modelMatrix;

void main () 
{
	vs_out.texCoords = aTexCoords;
	vs_out.fragPos = (modelMatrix * vec4(aPos, 1.0)).xyz;
	vs_out.clipSpace = projectionViewMatrix * modelMatrix * vec4(aPos, 1.0);

	gl_Position = vs_out.clipSpace;
}
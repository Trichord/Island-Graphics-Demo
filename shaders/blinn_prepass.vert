#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 3) in vec2 aTexCoords;

layout (std140) uniform matrices 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionViewMatrix;
	layout (offset = 128) vec4 cameraPosition;
};

out VS_OUT 
{
	vec3 fragPos;
	vec3 normal;
	vec2 texCoords;
} vs_out;

uniform mat4 modelMatrix;
uniform mat3 normalMatrix; // transpose(inverse(mat3(modelMatrix)));

void main() 
{
	vs_out.texCoords = aTexCoords;
	vs_out.fragPos = (modelMatrix * vec4(aPos, 1.0)).xyz;
	vs_out.normal = normalize(normalMatrix * aNormal);
	
	gl_Position = projectionViewMatrix * modelMatrix * vec4(aPos, 1.0);
}
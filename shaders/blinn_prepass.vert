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
uniform mat3 normalMatrix;
uniform vec4 plane;

void main() 
{
	vec4 worldPos = modelMatrix * vec4(aPos, 1.0);

	vs_out.texCoords = aTexCoords;
	vs_out.fragPos = worldPos.xyz;
	vs_out.normal = normalize(normalMatrix * aNormal);

	gl_ClipDistance[0] = dot(worldPos, plane);
	gl_Position = projectionViewMatrix * worldPos;
}
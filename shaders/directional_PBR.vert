#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aTangent;
layout (location = 3) in vec2 aTexCoords;

layout (std140) uniform matrices 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
};

out VS_OUT 
{
	vec3 cameraPos;
	vec3 lightDir;
	vec3 fragPos;
	vec2 texCoords;
} vs_out;

uniform mat4 modelMatrix;
uniform mat3 normalMatrix; // transpose(inverse(mat3(modelMatrix)));
uniform vec3 lightDir;

void main () 
{
	//TBN matrix for normal map
	vec3 T = normalize(normalMatrix * aTangent.xyz);
	vec3 N = normalize(normalMatrix * aNormal);
	T = normalize(T - dot(T, N) * N); 
	vec3 B = normalize(cross(N, T) * aTangent.w);
	mat3 TBN = transpose(mat3(T, B, N));

	vs_out.texCoords = aTexCoords;
	vs_out.fragPos = TBN * (modelMatrix * vec4(aPos, 1.0)).xyz;
	vs_out.lightDir = TBN * lightDir;
	vs_out.cameraPos = TBN * cameraPosition.xyz;
	
	gl_Position = projectionViewMatrix * modelMatrix * vec4(aPos, 1.0);
}
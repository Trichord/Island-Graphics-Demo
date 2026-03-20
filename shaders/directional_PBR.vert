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

layout(binding = 0, std430) readonly buffer matricesSSBO {
    mat4 instances[];
};

layout(binding = 1, std430) readonly buffer indicesSSBO {
    uint indices[];
};

out VS_OUT 
{
	vec3 cameraPos;
	vec3 lightDir;
	vec3 fragPos;
	vec2 texCoords;
	vec4 worldLightFragPos;
	vec4 worldFragPos;
} vs_out;

uniform uint matricesOffset;
uniform uint indicesOffset;
uniform vec3 lightDir;
uniform mat4 lightViewProjection;

void main () 
{
	uint instanceIndex = indices[indicesOffset + gl_InstanceID];
	mat4 modelMatrix = instances[matricesOffset + instanceIndex];
	mat3 normalMatrix = mat3(modelMatrix);

	//TBN matrix for normal map
	vec3 T = normalize(normalMatrix * aTangent.xyz);
	vec3 N = normalize(normalMatrix * aNormal);
	T = normalize(T - dot(T, N) * N); 
	vec3 B = normalize(cross(N, T) * aTangent.w);
	mat3 TBN = transpose(mat3(T, B, N));

	vs_out.texCoords = aTexCoords;
	vs_out.worldFragPos = modelMatrix * vec4(aPos, 1.0);
	vs_out.fragPos = TBN * vs_out.worldFragPos.xyz;
	vs_out.worldLightFragPos = lightViewProjection * vs_out.worldFragPos;
	vs_out.lightDir = TBN * lightDir;
	vs_out.cameraPos = TBN * cameraPosition.xyz;
	
	gl_Position = projectionViewMatrix * modelMatrix * vec4(aPos, 1.0);
}
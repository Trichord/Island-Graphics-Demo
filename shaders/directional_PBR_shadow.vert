#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aTangent;
layout (location = 3) in vec2 aTexCoords;

layout (std140) uniform uniformData 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
	layout (offset = 272) float time;
};


layout(binding = 0, std430) readonly buffer matricesSSBO 
{
    mat4 instances[];
};

layout(binding = 1, std430) readonly buffer indicesSSBO 
{
    uint indices[];
};

struct Metadata
{
	int albedoArray;
	int albedoLayer;
	int armArray;
	int armLayer;
	int normalArray;
	int normalLayer;
	int cubemapArray;
	int cubemapLayer;
	uint matricesBufferBaseIndex;
	uint visibleInstancesBufferBaseIndex;
};

layout(binding = 2, std430) readonly buffer metadataSSBO
{
	Metadata metadataBuffer[];
};

out vsOut 
{
	vec3 cameraPos;
	vec3 lightDir;
	vec3 tangentFragPos;
	vec2 texCoords;
	vec4 viewFragPos;
	vec4 worldFragPos;
} vOut;

flat out int albedoArray;
flat out int albedoLayer;

flat out int armArray;
flat out int armLayer;

flat out int normalArray;
flat out int normalLayer; 

uniform vec3 lightDir;
uniform mat4 lightViewProjection;

void main() 
{
	Metadata metadata = metadataBuffer[gl_BaseInstance];

	uint matricesIndex = metadata.matricesBufferBaseIndex;
	uint instanceIndex = indices[metadata.visibleInstancesBufferBaseIndex + gl_InstanceID];

	mat4 modelMatrix = instances[matricesIndex + instanceIndex];
	mat3 normalMatrix = mat3(modelMatrix);

	//TBN matrix for normal map
	vec3 T = normalize(normalMatrix * aTangent.xyz);
	vec3 N = normalize(normalMatrix * aNormal);
	T = normalize(T - dot(T, N) * N); 
	vec3 B = normalize(cross(N, T) * aTangent.w);
	mat3 TBN = transpose(mat3(T, B, N));

	vOut.worldFragPos = modelMatrix * vec4(aPos, 1.0);
	vOut.viewFragPos = viewMatrix * vOut.worldFragPos;
	vOut.tangentFragPos = TBN * vOut.worldFragPos.xyz;

	vOut.lightDir = TBN * lightDir;
	vOut.cameraPos = TBN * cameraPosition.xyz;

	vOut.texCoords = aTexCoords;

	albedoArray = metadata.albedoArray;
	albedoLayer = metadata.albedoLayer;
	
	armArray = metadata.armArray;
	armLayer = metadata.armLayer;

	normalArray = metadata.normalArray;
	normalLayer = metadata.normalLayer;
	
	gl_Position = projectionViewMatrix * modelMatrix * vec4(aPos, 1.0);
}
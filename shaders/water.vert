#version 460 core

layout (location = 0) in vec3 aPos;
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

layout(binding = 0, std430) readonly buffer matricesSSBO {
    mat4 instances[];
};

layout(binding = 1, std430) readonly buffer indicesSSBO {
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
	vec2 texCoords;
	vec3 fragPos;
	vec4 clipSpace;
} vOut;

flat out int armArray;
flat out int armLayer;

flat out int normalArray;
flat out int normalLayer;

flat out int cubeArray;
flat out int cubeLayer;

flat out float scale;

void main () 
{
	Metadata metadata = metadataBuffer[gl_BaseInstance];

	uint matricesIndex = metadata.matricesBufferBaseIndex;
	uint instanceIndex = indices[metadata.visibleInstancesBufferBaseIndex + gl_InstanceID];

	mat4 modelMatrix = instances[matricesIndex + instanceIndex];
	mat3 normalMatrix = mat3(modelMatrix);

	vOut.texCoords = aTexCoords;
	vOut.fragPos = (modelMatrix * vec4(aPos, 1.0)).xyz;
	vOut.clipSpace = projectionViewMatrix * modelMatrix * vec4(aPos, 1.0);

	armArray = metadata.armArray;
	armLayer = metadata.armLayer;

	normalArray = metadata.normalArray;
	normalLayer = metadata.normalLayer;

	cubeArray = metadata.cubemapArray;
	cubeLayer = metadata.cubemapLayer;

	scale = length(modelMatrix[0]);

	gl_Position = vOut.clipSpace;
}
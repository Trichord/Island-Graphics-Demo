#version 460 core

layout (location = 0) in vec3 aPos;

layout(binding = 0, std430) readonly buffer matricesSSBO 
{
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

uniform mat4 lightViewProjection;

void main()
{	
	Metadata metadata = metadataBuffer[gl_BaseInstance];

	uint matricesIndex = metadata.matricesBufferBaseIndex;
	uint instanceIndex = indices[metadata.visibleInstancesBufferBaseIndex + gl_InstanceID];

	mat4 modelMatrix = instances[matricesIndex + instanceIndex];

	gl_Position = lightViewProjection * modelMatrix * vec4(aPos, 1.0);
}
#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 3) in vec2 aTexCoords;

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
	Metadata metadata[];
};

uniform mat4 lightViewProjection;
uniform bool useIndices;

out vec2 texCoords;

flat out int albedoArray;
flat out int albedoLayer;

void main()
{	
	Metadata primitiveData = metadata[gl_BaseInstance];

	uint matricesIndex = primitiveData.matricesBufferBaseIndex;
	mat4 modelMatrix;

	if (useIndices)
	{
		uint instanceIndex = indices[primitiveData.visibleInstancesBufferBaseIndex + gl_InstanceID];
		modelMatrix = instances[matricesIndex + instanceIndex];
	}
	else
		modelMatrix = instances[matricesIndex + gl_InstanceID];

	vec4 vertexPos = lightViewProjection * modelMatrix * vec4(aPos, 1.0);
	texCoords = aTexCoords;

	albedoArray = primitiveData.albedoArray;
	albedoLayer = primitiveData.albedoLayer;

	gl_Position = vertexPos;


}
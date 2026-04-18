#version 460 core

layout (location = 0) in vec3 aPos;

layout(binding = 0, std430) readonly buffer matricesSSBO 
{
    mat4 instances[];
};

layout(binding = 1, std430) readonly buffer indicesSSBO {
    int indices[];
};

struct Metadata
{
	int albedoArray;
	int albedoLayer;
	int armArray;
	int armLayer;
	int normalArray;
	int normalLayer;
	int matricesBufferBaseIndex;
};

layout(binding = 2, std430) readonly buffer metadataSSBO
{
	Metadata metadata[];
};

uniform int metadataIndex;
uniform int indicesOffset;

uniform mat4 lightViewProjection;
uniform bool useIndices;

void main()
{	
	Metadata primitiveData = metadata[metadataIndex];

	int matricesIndex = primitiveData.matricesBufferBaseIndex;
	mat4 modelMatrix;

	if (useIndices)
	{
		int instanceIndex = indices[indicesOffset + gl_InstanceID];
		modelMatrix = instances[matricesIndex + instanceIndex];
	}
	else
		modelMatrix = instances[matricesIndex + gl_InstanceID];

	vec4 vertexPos = lightViewProjection * modelMatrix * vec4(aPos, 1.0);

	gl_Position = vertexPos;
}
#version 460 core

// Types
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
	uint visibleInstancesBufferBaseIndex;
};

// Attributes
layout (location = 0) in vec3 aPos;

// UBOs
layout (std140, binding = 1) uniform cascadeShadow
{
    vec4 splits;
    mat4 lightMatrices[3];
};

// SSBOs
layout(binding = 0, std430) restrict readonly buffer matricesSSBO 
{
    mat4 instances[];
};

layout(binding = 1, std430) restrict readonly buffer indicesSSBO {
    uint indices[];
};

layout(binding = 2, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadataBuffer[];
};

// In
uniform int cascade;

void main()
{	
	Metadata metadata = metadataBuffer[gl_BaseInstance];
	uint matrixIndex = indices[metadata.visibleInstancesBufferBaseIndex + gl_InstanceID];

	mat4 modelMatrix = instances[matrixIndex];

	gl_Position = lightMatrices[cascade] * modelMatrix * vec4(aPos, 1.0);
}
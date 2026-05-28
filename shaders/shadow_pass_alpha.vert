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
	uint visibleInstancesBaseIndex;
};

// Attributes
layout (location = 0) in vec3 aPos;
layout (location = 3) in vec2 aTexCoords;

// UBOs
layout (std140, binding = 2) uniform cascadeShadows
{
	vec4 near;
    vec4 far;
    mat4 lightMatrices[3];
};

// SSBOs
layout(binding = 2, std430) restrict readonly buffer matricesSSBO 
{
   mat4 matrices[];
};

layout(binding = 3, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadata[];
};

layout(binding = 7, std430) restrict readonly buffer visibleInstancesSSBO 
{
   uint instances[];
};

// In
uniform int cascade;

// Out
out vec2 texCoords;

flat out int metadataIndex;

void main()
{	
	Metadata meta = metadata[gl_BaseInstance];
	uint matrixIndex = instances[meta.visibleInstancesBaseIndex + gl_InstanceID];

	mat4 modelMatrix = matrices[matrixIndex];

	texCoords = aTexCoords;
	metadataIndex = gl_BaseInstance;

	gl_Position = lightMatrices[cascade] * modelMatrix * vec4(aPos, 1.0);;
}
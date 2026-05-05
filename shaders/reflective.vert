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
layout (location = 1) in vec3 aNorm;

// UBOs
layout (std140) uniform uniformData 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
	layout (offset = 272) float time;
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

// Out
flat out int metadataIndex;

out vec3 normal;
out vec3 fragPos;
out vec3 position;

void main() 
{
	Metadata meta = metadata[gl_BaseInstance];
	uint matrixIndex = instances[meta.visibleInstancesBufferBaseIndex + gl_InstanceID];

	mat4 modelMatrix = matrices[matrixIndex];
	mat3 normalMatrix = mat3(modelMatrix);

	normal = normalMatrix * aNorm;
	position = vec3(modelMatrix * vec4(aPos, 1.0f));

	metadataIndex = gl_BaseInstance;

	gl_Position = projectionViewMatrix * vec4(position, 1.0f);
}
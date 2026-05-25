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
layout (std140, binding = 0) uniform cameraData 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
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
flat out float scale;
flat out int metadataIndex;

out vsOut 
{
	vec2 texCoords;
	vec3 fragPos;
	vec4 clipSpace;
} vOut;

void main() 
{
	Metadata meta = metadata[gl_BaseInstance];
	uint matrixIndex = instances[meta.visibleInstancesBaseIndex + gl_InstanceID];

	mat4 modelMatrix = matrices[matrixIndex];
	mat3 normalMatrix = mat3(modelMatrix);

	vec4 worldPos = modelMatrix * vec4(aPos, 1.0);

	vOut.texCoords = aTexCoords;
	vOut.fragPos = worldPos.xyz;
	vOut.clipSpace = projectionViewMatrix * worldPos;

	metadataIndex = gl_BaseInstance;
	scale = length(modelMatrix[0]);

	gl_Position = vOut.clipSpace;
}
#version 460 core

#include "binding_points.glsl"
#include "types.glsl"
#include "ubo.glsl"

// Attributes
layout (location = 0) in vec3 aPos;
layout (location = 3) in vec2 aTexCoords;

// SSBOs
layout(binding = SSBO_BINDING_MATRICES, std430) restrict readonly buffer matricesSSBO
{
   mat4 matrices[];
};

layout(binding = SSBO_BINDING_METADATA, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadata[];
};

layout(binding = SSBO_BINDING_VISIBLE_INSTANCES, std430) restrict readonly buffer visibleInstancesSSBO 
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
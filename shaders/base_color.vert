#version 460 core

#include "shared/binding_points.glsl"
#include "shared/types.glsl"
#include "shared/ubo.glsl"

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
out vec2 texCoords;
flat out int array;
flat out int layer;

void main() 
{
	Metadata meta = metadata[gl_BaseInstance];
	uint matrixIndex = instances[meta.visibleInstancesBaseIndex + gl_InstanceID];
	mat4 modelMatrix = matrices[matrixIndex];

	texCoords = aTexCoords;
	array = meta.albedoArray;
	layer = meta.albedoLayer;

	gl_Position = viewProjectionMatrix * modelMatrix * vec4(aPos, 1.0);
}
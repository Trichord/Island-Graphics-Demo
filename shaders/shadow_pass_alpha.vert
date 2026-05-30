#version 460 core

#include "binding_points.glsl"
#include "types.glsl"
#include "ubo.glsl"

// Attributes
layout (location = 0) in vec4 aPos;
layout (location = 1) in vec2 aTexCoords;

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

	gl_Position = lightMatrices[cascade] * modelMatrix * aPos;
}
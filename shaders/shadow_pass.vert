#version 460 core

#include "binding_points.glsl"
#include "types.glsl"
#include "ubo.glsl"

// Attributes
layout (location = 0) in vec4 aPos;
#ifdef ALPHA_TESTED
layout (location = 1) in vec2 aTexCoords;
#endif

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

#ifdef ALPHA_TESTED
// Out
out vec2 texCoords;

flat out int metadataIndex;
#endif

void main()
{	
	Metadata meta = metadata[gl_BaseInstance];
	uint matrixIndex = instances[meta.visibleInstancesBaseIndex + gl_InstanceID];

	mat4 modelMatrix = matrices[matrixIndex];

	#ifdef ALPHA_TESTED
	texCoords = aTexCoords;
	metadataIndex = gl_BaseInstance;
	#endif

	gl_Position = lightMatrices[cascade] * modelMatrix * aPos;
}
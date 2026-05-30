#version 460 core

#include "binding_points.glsl"
#include "types.glsl"
#include "ubo.glsl"

// Attributes
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNorm;

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
flat out int metadataIndex;

out vec3 normal;
out vec3 fragPos;
out vec3 position;

void main() 
{
	Metadata meta = metadata[gl_BaseInstance];
	uint matrixIndex = instances[meta.visibleInstancesBaseIndex + gl_InstanceID];

	mat4 modelMatrix = matrices[matrixIndex];
	mat3 normalMatrix = mat3(modelMatrix);

	normal = normalMatrix * aNorm;
	position = vec3(modelMatrix * vec4(aPos, 1.0f));

	metadataIndex = gl_BaseInstance;

	gl_Position = projectionViewMatrix * vec4(position, 1.0f);
}
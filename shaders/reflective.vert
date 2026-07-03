#version 460 core

#include "shared/binding_points.glsl"
#include "shared/types.glsl"
#include "shared/ubo.glsl"

// Attributes
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aNormal;

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

	normal = normalMatrix * aNormal.xyz;
	position = vec3(modelMatrix * vec4(aPos, 1.0f));

	metadataIndex = gl_BaseInstance;

	gl_Position = viewProjectionMatrix * vec4(position, 1.0f);
}
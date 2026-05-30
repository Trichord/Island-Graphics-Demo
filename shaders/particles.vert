#version 460 core

#include "binding_points.glsl"
#include "types.glsl"
#include "ubo.glsl"

// Constants
const float pointSize = 0.025;

// Attributes
layout (location = 0) in vec3 aPos;
layout (location = 2) in vec4 aColor;

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
uniform int screenHeight;
uniform float fov;

// Out
flat out vec4 color;

void main() 
{
	Metadata meta = metadata[gl_BaseInstance];
	uint matrixIndex = instances[meta.visibleInstancesBaseIndex + gl_InstanceID];

	mat4 modelMatrix = matrices[matrixIndex];
	mat3 normalMatrix = mat3(modelMatrix);

	color = aColor;
	
	gl_Position = projectionViewMatrix * modelMatrix * vec4(aPos, 1.0);
	gl_PointSize = (screenHeight * pointSize) / (gl_Position.w * tan(radians(fov) * 0.5));
}
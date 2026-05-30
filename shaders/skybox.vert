#version 460 core

#include "binding_points.glsl"
#include "types.glsl"
#include "ubo.glsl"

// Attributes
layout (location = 0) in vec3 aPos;

// SSBOs
layout(binding = SSBO_BINDING_METADATA, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadata[];
};

// Out
flat out int metadataIndex;

out vec4 texCoords;

void main() 
{
	texCoords = inverseViewProjectMatrix * vec4(aPos, 1.0);
	metadataIndex = gl_BaseInstance;

	gl_Position = vec4(aPos, 1.0);
}
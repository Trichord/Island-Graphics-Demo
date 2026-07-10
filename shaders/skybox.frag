#version 460 core

#include "shared/binding_points.glsl"
#include "shared/samplers.glsl"
#include "shared/types.glsl"
#include "shared/ubo.glsl"

// SSBOs
layout(binding = SSBO_BINDING_METADATA, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadata[];
};

// In
flat in int metadataIndex;

in vec4 texCoords;

// Out
out vec4 fragColor;

void main() 
{
	Metadata meta = metadata[metadataIndex];

	vec3 skyboxCoords = texCoords.xyz / texCoords.w;
	vec3 color = textureLod(cubemaps[meta.cubemapArray], vec4(skyboxCoords, meta.cubemapLayer), 0).rgb;
	
	fragColor = vec4(color, 1.0); 
}
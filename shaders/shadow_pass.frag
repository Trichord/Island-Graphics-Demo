#version 460 core

#ifdef ALPHA_TESTED_SHADOWS
#include "shared/binding_points.glsl"
#include "shared/samplers.glsl"
#include "shared/types.glsl"

// SSBOs
layout(binding = SSBO_BINDING_METADATA, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadata[];
};

// In
in vec2 texCoords;
in flat int metadataIndex;
#endif

void main()
{	
	#ifdef ALPHA_TESTED_SHADOWS
	Metadata meta = metadata[metadataIndex];
	int albedoArray = meta.albedoArray;
	int albedoLayer = meta.albedoLayer;

	float alpha = texture(textures[albedoArray], vec3(texCoords, albedoLayer)).a;

	if (alpha < 0.1)
		discard;
	#endif
}
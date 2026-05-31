#version 460 core

#ifdef ALPHA_TESTED
#include "binding_points.glsl"
#include "textures.glsl"
#include "types.glsl"

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
	#ifdef ALPHA_TESTED
	Metadata meta = metadata[metadataIndex];
	int albedoArray = meta.albedoArray;
	int albedoLayer = meta.albedoLayer;

	float alpha = texture(textures[albedoArray], vec3(texCoords, albedoLayer)).a;

	if (alpha < 0.1)
		discard;
	#endif
}
#version 460 core

#include "binding_points.glsl"
#include "textures.glsl"
#include "types.glsl"
#include "ubo.glsl"

// SSBOs
layout(binding = SSBO_BINDING_METADATA, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadata[];
};

// In
uniform float gamma;

flat in int metadataIndex;

in vec4 texCoords;

// Out
out vec4 fragColor;

void main() 
{
	Metadata meta = metadata[metadataIndex];

	vec3 skyboxCoords = texCoords.xyz / texCoords.w;
	vec3 color = texture(cubemaps[meta.cubemapArray], vec4(skyboxCoords, meta.cubemapLayer)).rgb;
	
	// Gamma correction
	color = pow(color, vec3(1.0/gamma));
	
	fragColor = vec4(color, 1.0); 
}
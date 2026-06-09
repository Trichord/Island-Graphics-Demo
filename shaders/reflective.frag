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
in flat int metadataIndex;

in vec3 normal;
in vec3 position;

// Out
out vec4 fragColor;

void main() 
{
	Metadata meta = metadata[metadataIndex];

	vec3 viewDir = normalize(position - cameraPosition.xyz);
	vec3 reflectDir = reflect(viewDir, normalize(normal));

	vec3 color = texture(cubemaps[meta.cubemapArray], vec4(reflectDir, meta.cubemapLayer)).rgb;
	
	fragColor = vec4(color, 1.0); 
}
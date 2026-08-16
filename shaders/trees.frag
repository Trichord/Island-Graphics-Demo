#version 460 core

#include "shared/binding_points.glsl"
#include "shared/constants.glsl"
#include "shared/foliage.glsl"
#include "shared/pbr.glsl"
#include "shared/samplers.glsl"
#include "shared/types.glsl"
#include "shared/ubo.glsl"

#ifdef ALPHA_TESTED_SHADOWS
#include "shared/shadows.glsl"
#endif

// Constants
const float minShadow = 0.3;

// SSBOs
layout(binding = SSBO_BINDING_METADATA, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadata[];
};

// In
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientLight;

flat in int metadataIndex;

in vec4 worldPos;
in vec3 normal;
in vec2 texCoords;
#ifdef ALPHA_TESTED_SHADOWS
in vec4 viewPos;
#endif

// Out
out vec4 fragColor;

void main() 
{
	Metadata meta = metadata[metadataIndex];

	// Materials
	vec4 albedo = texture(textures[meta.albedoArray], vec3(texCoords, meta.albedoLayer));

	// Scale alpha by mip level
    vec2 texSize = textureSize(textures[meta.albedoArray], 0).xy;
    albedo.a *= 1 + max(0, calcMipLevel(texCoords * texSize)) * mipScale;
	albedo.a = rescaleAlpha(albedo.a);
	
	float NdotL = max(dot(normalize(normal), normalize(-lightDir)), 0.0);
	vec3 diffuse = albedo.rgb * inversePi * NdotL;

	#ifdef ALPHA_TESTED_SHADOWS
	int layer = calculateCascadeIndex(-viewPos.z);

	// Fully shadowed leaves look too dark, cheap effect to brighten the canopies
	float shadow = clamp(shadowContribution(lightMatrices[layer], worldPos, layer), minShadow, 1.0);

	vec3 accumulatedLight = diffuse * lightColor * shadow;

	#else
	vec3 accumulatedLight = diffuse * lightColor;
	#endif
	
	// Ambient
	vec3 ambient = ambientLight * albedo.rgb;
	
	// Final color
	vec3 color = ambient + accumulatedLight;

	fragColor = vec4(color, albedo.a);
}



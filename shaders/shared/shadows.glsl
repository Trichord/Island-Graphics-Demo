// Functions for calculating shadow contribution

#include "shared/samplers.glsl"
#include "shared/ubo.glsl"

int calculateCascadeIndex(float depth)
{
	if (far.x > depth)
		return 0;

	else if (far.y > depth)
		return 1;

	else
		return 2;
}

float shadowContribution(mat4 lightMatrix, vec4 worldFragPos, vec3 normal, int layer)
{	
	// Texels
	const float normalBias = 1.5;
	const float constBias = 0.5;
	const float slopeBias = 1.0;

	float texel = texelSizes[layer];
	vec3 L = normalize(-lightDir.xyz);
	float NdotL = clamp(dot(normal, L), 0.0, 1.0);

	// Credit: Ignacio Castano https://www.ludicon.com/castano/blog/articles/shadow-mapping-summary-part-1/
	float offsetScaleN = sqrt(1.0 - NdotL * NdotL); // sin(acos(NdotL))
	float offsetScaleL = min(offsetScaleN / NdotL, 2.0); // tan(acos(NdotL))

	// Move the world position along the normal and towards the light, scaled by texel size
	vec3 pos = worldFragPos.xyz + normal * (texel * offsetScaleN * normalBias) + L * (texel * (constBias + offsetScaleL * slopeBias));
	vec3 shadowCoords = vec3(lightMatrix * vec4(pos, 1.0)) * 0.5 + 0.5;
	return texture(shadowMap, vec4(shadowCoords.xy, float(layer), shadowCoords.z));
}
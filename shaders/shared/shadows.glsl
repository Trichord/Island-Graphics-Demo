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

bool betweenCascades(float depth)
{
	bool firstBlend  = depth > near.y && depth < far.x;
	bool secondBlend = depth > near.z && depth < far.y;
	return firstBlend || secondBlend;
}

float shadowContribution(vec4 worldFragPos, vec3 normal, float fragDepth)
{	
	// Texels
	const float normalBias = 1.5;
	const float constBias = 0.5;
	const float slopeBias = 1.0;

	int layer = calculateCascadeIndex(fragDepth);
	mat4 lightMatrix = lightMatrices[layer];

	float texel = texelSizes[layer];
	vec3 L = normalize(-lightDir.xyz);
	float NdotL = clamp(dot(normal, L), 0.0, 1.0);

	// Credit: Ignacio Castano https://www.ludicon.com/castano/blog/articles/shadow-mapping-summary-part-1/
	float offsetScaleN = sqrt(1.0 - NdotL * NdotL); // sin(acos(NdotL))
	float offsetScaleL = min(offsetScaleN / NdotL, 2.0); // tan(acos(NdotL))

	// Move the world position along the normal and towards the light, scaled by texel size
	vec3 pos = worldFragPos.xyz + normal * (texel * offsetScaleN * normalBias) + L * (texel * (constBias + offsetScaleL * slopeBias));
	vec3 shadowCoords = vec3(lightMatrix * vec4(pos, 1.0)) * 0.5 + 0.5;
	float shadow = texture(shadowMap, vec4(shadowCoords.xy, float(layer), shadowCoords.z));

	float blendWeight = 0.0;
	if (layer == 0 && fragDepth > near.y)
		blendWeight = (fragDepth - near.y) / (far.x - near.y);
	else if (layer == 1 && fragDepth > near.z)
		blendWeight = (fragDepth - near.z) / (far.y - near.z);

	if (blendWeight > 0.0)
	{
		mat4 nextLightMatrix = lightMatrices[layer + 1];
		vec3 nextShadowCoords = vec3(nextLightMatrix * vec4(pos, 1.0)) * 0.5 + 0.5;
		float next = texture(shadowMap, vec4(nextShadowCoords.xy, float(layer + 1), nextShadowCoords.z));
		shadow = mix(shadow, next, blendWeight);
	}

	return shadow;
}
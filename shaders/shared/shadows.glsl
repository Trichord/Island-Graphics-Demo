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
	float depthBiasScale = 0.0003;
	float normalBiasScale = 0.001;

	vec3 L = normalize(-lightDir.xyz);
	float NdotL = clamp(dot(normal, L), 0.0, 1.0);

	// Depth bias along normal, scaled by cascade 
	// Credit: Ignacio Castano https://www.ludicon.com/castano/blog/articles/shadow-mapping-summary-part-1/
	float offsetScaleN = sqrt(1.0 - NdotL * NdotL); // sin(acos(NdotL))
	float offsetScaleL = min(offsetScaleN / NdotL, 2.0); // tan(acos(NdotL))

	vec4 pos = worldFragPos + vec4(normal, 0.0) * offsetScaleN * normalBiasScale;
	vec3 shadowCoords = vec3(lightMatrix * pos);
	shadowCoords = shadowCoords * 0.5 + 0.5;
	shadowCoords.z -= offsetScaleL * depthBiasScale;

	return texture(shadowMap, vec4(shadowCoords.xy, float(layer), shadowCoords.z));
}
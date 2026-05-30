// Functions for calculating shadow contribution

#include "textures.glsl"
#include "ubo.glsl"

int calculateCascadeIndex(float depth)
{
	if (far.x > depth)
		return 0;

	else if (far.y > depth)
		return 1;

	else
		return 2;
}

float ShadowContribution(mat4 lightMatrix, vec4 worldFragPos, int layer)
{	
	vec3 shadowCoords = vec3(lightMatrix * worldFragPos);
	shadowCoords = shadowCoords * 0.5 + 0.5;

	return texture(shadowMap, vec4(shadowCoords.xy, float(layer), shadowCoords.z));
}
#version 460 core

out vec4 fragColor;

uniform sampler2D albedoMap;
uniform sampler2D armMap;
uniform sampler2D normalMap;
uniform sampler2D shadowMap;
uniform vec3 ambientLight;
uniform vec3 lightColor;
uniform float gamma;

const float minVariance = 0.0001;

//Positions in tangent view space
in VS_OUT {
	vec3 cameraPos;
	vec3 lightDir;
	vec3 fragPos;
	vec2 texCoords;
	vec4 worldLightFragPos;
	vec4 worldFragPos;
} fs_in;

//Andrew Lauritzen - "Summed-Area Variance Shadow Maps"
float ChebyshevUpperBound(vec2 moments, float t)
{
  // One-tailed inequality valid if t > Moments.x
  float p = step(t, moments.x);
  // Compute variance.
  float variance = moments.y - (moments.x * moments.x);
  variance = max(variance, minVariance);
  // Compute probabilistic upper bound.
  float d = t - moments.x;
  float pMax = variance / (variance + d * d);
  return max(p, pMax);
}

float ShadowContribution(vec2 LightTexCoord, float DistanceToLight)
{
  // Read the moments from the variance shadow map.
  vec2 moments = texture(shadowMap, LightTexCoord).rg;
  // Compute the Chebyshev upper bound.
  return ChebyshevUpperBound(moments, DistanceToLight);
}

void main() {

	//Shadows
	vec3 shadowCoords = fs_in.lightFragPos.xyz / fs_in.lightFragPos.w;
	shadowCoords = shadowCoords * 0.5 + 0.5;
	//float shadow = ShadowContribution(shadowCoords.xy, shadowCoords.z);

}
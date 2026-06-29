// Functions for Cook-Torrance BRDF

#include "shared/constants.glsl"

// Trowbridge-Reitz GGX for normal distribution function
float distributionGGX(float NdotH, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH2 = NdotH * NdotH;
	float denom = fma(NdotH2, a2 - 1.0, 1.0);
	float denom2 = denom * denom;

	return a2 * inversePi / denom2;
}

// Smith's method with Schlick-GGX for geometric shadowing.  k = (a + 1)^2 / 8
float geometrySmith(float NdotV, float NdotL, float roughness)
{
	float r = roughness + 1.0;
	float k = r * r * 0.125;
	float invK = 1.0 - k;

	// ggx1 * ggx2
	float numerator = NdotL * NdotV;
	float denom = (NdotL * invK + k) * (NdotV * invK + k);

	return numerator / denom;
}

// Fresnel-Schlick approximation (unrolled x^5)
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	float x = clamp(1.0 - cosTheta, 0.0, 1.0);
	float x2 = x * x;
	float x4 = x2 * x2;
	float x5 = x4 * x;

	return F0 + (1.0 - F0) * x5;
}

// Burely diffuse (unrolled)
float burleyDiffuseFactor(float FD90, float NdotL, float NdotV)
{
	float L = 1.0 - NdotL;
	float L2 = L * L;
	float L4 = L2 * L2;
	float L5 = L4 * L;

	float V = 1.0 - NdotV;
	float V2 = V * V;
	float V4 = V2 * V2;
	float V5 = V4 * V;

	float FDminusOne = FD90 - 1.0;

	return (1.0 + FDminusOne * L5) * (1.0 + FDminusOne * V5); 
}
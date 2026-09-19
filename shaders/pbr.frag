#version 460 core

#include "shared/binding_points.glsl"
#include "shared/constants.glsl"
#include "shared/samplers.glsl"
#include "shared/types.glsl"
#include "shared/ubo.glsl"

#ifdef ALPHA_TESTED_SHADOWS
#include "shared/shadows.glsl"
#endif

#ifdef REFLECTIVE
const float reflectionDepth = 1.0;
#endif

// SSBOs
layout(binding = SSBO_BINDING_METADATA, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadata[];
};

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

// In
flat in int metadataIndex;

in vsOut 
{
	vec4 worldFragPos;
	vec3 tangentCameraPos;
	vec3 tangentLightDir;
	vec3 tangentFragPos;
	vec3 geoNormal;
	vec2 texCoords;
	float viewFragDepth;
} fsIn;

// Out
out vec4 fragColor;

void main() 
{
	Metadata meta = metadata[metadataIndex];
	vec2 texCoords = fsIn.texCoords;

	// Vectors
	vec3 V = normalize(fsIn.tangentCameraPos -fsIn.tangentFragPos);
	vec3 L = normalize(-fsIn.tangentLightDir);
	vec3 H = normalize(L + V);

	vec3 N;
	N.xy = texture(textures[meta.normalArray], vec3(texCoords, meta.normalLayer)).rg * 2.0 - 1.0;
	N.z = sqrt(1 - dot(N.xy, N.xy));
	N = normalize(N);

	#ifdef REFLECTIVE
	vec3 t = reflect(-V, N);
	t.z = reflectionDepth * texture(noise, fsIn.texCoords).r / abs(t.z);
	texCoords += t.xy * t.z;
	#endif

	// Dot products
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float NdotH = max(dot(N, H), 0.0);
	float HdotV = max(dot(H, V), 0.0);

	// Material properties, remap normal from [0, 1] to [-1, 1]
	vec4 albedo = texture(textures[meta.albedoArray], vec3(texCoords, meta.albedoLayer));
	vec3 ARM = texture(textures[meta.armArray], vec3(texCoords, meta.armLayer)).rgb;

	float ao = ARM.r;
	float roughness = ARM.g;
	float metallic = ARM.b;

	// Fresnel-Shlick base reflectivity
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo.rgb, metallic);

	// Cook-Torrance BRDF
	float NDF = distributionGGX(NdotH, roughness);
	float G = geometrySmith(NdotV, NdotL, roughness);
	vec3 F = fresnelSchlick(HdotV, F0);

	// Avoid division by 0, using a small constant
	vec3 numerator = NDF * G * F;
	float denom = max(4.0 * NdotL * NdotV, 0.0001);
	vec3 specular = numerator / denom;

	// Burley diffuse
	float FD90 = 0.5 + 2 * roughness * (HdotV * HdotV);
	float burleyFactor = burleyDiffuseFactor(FD90, NdotL, NdotV);
	vec3 diffuse = (1.0 - metallic) * (albedo.rgb * burleyFactor * inversePi);

	vec3 Lo = (diffuse + specular) * lightColor.rgb * NdotL;
	vec3 ambient = ambientLight.rgb * albedo.rgb * ao;

	#ifdef ALPHA_TESTED_SHADOWS
	// Shadows
	float shadow = shadowContribution(fsIn.worldFragPos, normalize(fsIn.geoNormal), -fsIn.viewFragDepth);
	vec3 color = ambient + Lo * shadow;
	
	#else
	vec3 color = ambient + Lo;
	#endif

    fragColor = vec4(color, albedo.a);
}
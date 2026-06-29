#version 460 core

#include "shared/binding_points.glsl"
#include "shared/constants.glsl"
#include "shared/pbr.glsl"
#include "shared/textures.glsl"
#include "shared/types.glsl"
#include "shared/ubo.glsl"

#ifdef ALPHA_TESTED_SHADOWS
#include "shared/shadows.glsl"
#endif

// SSBOs
layout(binding = SSBO_BINDING_METADATA, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadata[];
};

// In
uniform vec3 lightColor;
uniform vec3 ambientLight;

flat in int metadataIndex;

// Positions in tangent view space
in vsOut 
{
	vec3 cameraPos;
	vec3 lightDir;
	vec3 tangentFragPos;
	vec2 texCoords;
	vec4 viewFragPos;
	vec4 worldFragPos;
} fsIn;

// Out
out vec4 fragColor;

void main() 
{
	Metadata meta = metadata[metadataIndex];

	// Material properties, remap normal from [0, 1] to [-1, 1]
	vec4 albedo = texture(textures[meta.albedoArray], vec3(fsIn.texCoords, meta.albedoLayer));
	vec3 ARM = texture(textures[meta.armArray], vec3(fsIn.texCoords, meta.armLayer)).rgb;

	vec3 N;
	N.xy = texture(textures[meta.normalArray], vec3(fsIn.texCoords, meta.normalLayer)).rg * 2.0 - 1.0;
	N.z = sqrt(max(1.0 - dot(N.xy, N.xy), 0.0));

	// Vectors
	N = normalize(N);
	vec3 V = normalize(fsIn.cameraPos -fsIn.tangentFragPos);
	vec3 L = normalize(-fsIn.lightDir);
	vec3 H = normalize(L + V);
	
	// Dot products
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float NdotH = max(dot(N, H), 0.0);
	float HdotV = max(dot(H, V), 0.0);

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

	// Specular and diffuse terms
	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;
	kD *= 1.0 - metallic;

	// Avoid division by 0, using a small constant
	vec3 numerator = NDF * G * F;
	float denom = max(4.0 * NdotL * NdotV, 0.0001);
	vec3 specular = numerator / denom;

	// Burley diffuse
	float FD90 = 0.5 + 2 * roughness * (HdotV * HdotV);
	float burleyFactor = burleyDiffuseFactor(FD90, NdotL, NdotV);
	vec3 fD = albedo.rgb * inversePi * burleyFactor;

	vec3 diffuse = kD * fD;

	// Outgoing light
	vec3 Lo = (diffuse + specular) * lightColor * NdotL;
	vec3 ambient = ambientLight * albedo.rgb * ao;

	#ifdef ALPHA_TESTED_SHADOWS
	// Pick shadow cascade off frag depth
	int layer = calculateCascadeIndex(-fsIn.viewFragPos.z);

	// Shadows
	float shadow = ShadowContribution(lightMatrices[layer], fsIn.worldFragPos, layer);
	float bias = max(0.05 * (1.0 - NdotL), 0.005);  
	vec3 color = ambient + Lo * (shadow + bias);
	
	#else
	vec3 color = ambient + Lo;
	#endif

	fragColor = vec4(color, albedo.a);
}
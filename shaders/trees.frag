#version 460 core

#include "shared/binding_points.glsl"
#include "shared/constants.glsl"
#include "shared/foliage.glsl"
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
	vec3 ARM = texture(textures[meta.armArray], vec3(texCoords, meta.armLayer)).rgb;
	vec3 normal = normalize(normal);

	float ao = ARM.r;
	float roughness = ARM.g;
	float metallic = ARM.b;

	// Scale alpha by mip level
    vec2 texSize = textureSize(textures[meta.albedoArray], 0).xy;
    albedo.a *= 1 + max(0, calcMipLevel(texCoords * texSize)) * mipScale;

	// Rescale alpha by partial derivative. Credit: Ben Golus
    albedo.a = (albedo.a - alphaCutoff) / max(fwidth(albedo.a), 0.0001) + 0.5;

	// Vectors
	vec3 N = normalize(normal);
	vec3 V = normalize(cameraPosition.xyz - worldPos.xyz);
	vec3 L = normalize(-lightDir);
	vec3 H = normalize(L + V);
	
	// Dot products
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float HdotV = max(dot(H, V), 0.0);

	// Alter diffuse by metallic-ness of the material
	vec3 kD = vec3(1.0);
	kD *= 1.0 - metallic;

	// Burley diffuse
	float FD90 = 0.5 + 2 * roughness * (HdotV * HdotV);
	float burleyFactor = burleyDiffuseFactor(FD90, NdotL, NdotV);
	vec3 diffuse = kD * albedo.rgb * inversePi * burleyFactor;

	#ifdef ALPHA_TESTED_SHADOWS
	int layer = calculateCascadeIndex(-viewPos.z);
	float shadow = ShadowContribution(lightMatrices[layer], worldPos, layer);
	vec3 accumulatedLight = diffuse * lightColor * shadow * NdotL;

	#else
	vec3 accumulatedLight = diffuse * lightColor * NdotL;
	#endif
	
	// Ambient
	vec3 ambient = ambientLight * albedo.rgb * ao;
	
	// Final color
	vec3 color = ambient + accumulatedLight;

	fragColor = vec4(color, albedo.a);
}



#version 460 core

#include "binding_points.glsl"
#include "constants.glsl"
#include "foliage.glsl"
#include "pbr.glsl"
#include "textures.glsl"
#include "tone_mapping.glsl"
#include "types.glsl"
#include "ubo.glsl"

#ifdef SHADOWS
#include "shadows.glsl"
#endif

// SSBOs
layout(binding = SSBO_BINDING_METADATA, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadata[];
};

// In
uniform float gamma;

uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientLight;

flat in int metadataIndex;

in vsOut 
{
	vec4 worldPos;
	vec3 normal;
	vec2 texCoords;

	#ifdef SHADOWS
	vec4 viewPos;
	#endif

} fsIn;

// Out
out vec4 fragColor;

void main() 
{
	Metadata meta = metadata[metadataIndex];

	// Materials
	vec4 albedo = texture(textures[meta.albedoArray], vec3(fsIn.texCoords, meta.albedoLayer));
	vec3 ARM = texture(textures[meta.armArray], vec3(fsIn.texCoords, meta.armLayer)).rgb;
	vec3 normal = normalize(fsIn.normal);

	float ao = ARM.r;
	float roughness = ARM.g;
	float metallic = ARM.b;

	// Scale alpha by mip level
    vec2 texSize = vec2(2048, 2048);
    albedo.a *= 1 + max(0, calcMipLevel(fsIn.texCoords * texSize)) * mipScale;

	// Rescale alpha by partial derivative. Credit: Ben Golus
    albedo.a = (albedo.a - alphaCutoff) / max(fwidth(albedo.a), 0.0001) + 0.5;

	// Vectors
	vec3 N = normalize(normal);
	vec3 V = normalize(cameraPosition.xyz - fsIn.worldPos.xyz);
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

	#ifdef SHADOWS

	int layer = calculateCascadeIndex(-fsIn.viewPos.z);
	float shadow = ShadowContribution(lightMatrices[layer], fsIn.worldPos, layer);
	vec3 accumulatedLight = diffuse * lightColor * shadow * NdotL;

	#else
	vec3 accumulatedLight = diffuse * lightColor * NdotL;
	#endif
	
	// Ambient
	vec3 ambient = ambientLight * albedo.rgb * ao;
	
	// Final color
	vec3 color = ambient + accumulatedLight;

	// Tone mapping
	color = ACESFilm(color);

	// Gamma correction
	color = pow(color, vec3(1.0/gamma));

	fragColor = vec4(color, albedo.a);
}



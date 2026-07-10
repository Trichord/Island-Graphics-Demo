#version 460 core

#include "shared/binding_points.glsl"
#include "shared/constants.glsl"
#include "shared/samplers.glsl"
#include "shared/types.glsl"
#include "shared/foliage.glsl"
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

in VS_OUT 
{
    vec3 normal;
	vec2 texCoords;
    float vertexHeight;

#ifdef ALPHA_TESTED_SHADOWS
	vec4 viewPos;
	vec4 worldPos;
#endif
} fsIn;

// Out
out vec4 fragColor;

void main() 
{
    Metadata meta = metadata[metadataIndex];

    vec4 albedo = texture(textures[meta.albedoArray], vec3(fsIn.texCoords, meta.albedoLayer));

    vec3 normalizedNormal = normalize(fsIn.normal);
    vec3 normalizedLightDir = normalize(-lightDir);

    // Diffuse
    float NdotL = dot(normalizedLightDir, normalizedNormal);
    float diffuse = NdotL * 0.5 + 0.5;
    diffuse = diffuse * diffuse;
    
    // Brighten up the base of the grass blades
    float diffuseBoost = (1.0 - smoothstep(0.0, 0.2, fsIn.vertexHeight)) * 0.5;
    diffuseBoost = diffuseBoost * (1.0 - smoothstep(0.0, 0.2, NdotL));
    diffuse += diffuseBoost;

	// Scale alpha by mip level 0
    vec2 texSize = vec2(textureSize(textures[meta.albedoArray], 0));
    albedo.a *= 1 + max(0, calcMipLevel(fsIn.texCoords * texSize)) * mipScale;
    albedo.a = rescaleAlpha(albedo.a);
    
    // Ambient
    vec3 ambient = ambientLight * lightColor * albedo.rgb;
    
    #ifdef ALPHA_TESTED_SHADOWS
	// Pick shadow cascade off frag depth
	int layer = calculateCascadeIndex(-fsIn.viewPos.z);
	float shadow = ShadowContribution(lightMatrices[layer], fsIn.worldPos, layer);
    shadow = clamp(shadow + 0.1, 0.0, 1.0);
    vec3 diffuseColor = diffuse * lightColor * shadow * albedo.rgb;
    
    #else
    vec3 diffuseColor = diffuse * lightColor * albedo.rgb;   
    #endif

    vec3 color = ambient + diffuseColor;
 
    fragColor = vec4(color, albedo.a);
}
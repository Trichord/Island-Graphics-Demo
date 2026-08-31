#version 460 core

#include "shared/binding_points.glsl"
#include "shared/constants.glsl"
#include "shared/samplers.glsl"
#include "shared/types.glsl"
#include "shared/foliage.glsl"
#include "shared/ubo.glsl"

// Constants
const float diffuseBoostFactor = 0.5;

// SSBOs
layout(binding = SSBO_BINDING_METADATA, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadata[];
};

flat in int metadataIndex;

in VS_OUT 
{
	vec3 normal;
	vec2 texCoords;
	float vertexHeight;
	float ao;
} fsIn;

// Out
out vec4 fragColor;

void main() 
{
    Metadata meta = metadata[metadataIndex];

    vec4 albedo = texture(textures[meta.albedoArray], vec3(fsIn.texCoords, meta.albedoLayer));

    vec3 N = normalize(fsIn.normal);
    vec3 L = normalize(-lightDir.xyz);

    // Scale alpha by mip level 0
    vec2 texSize = vec2(textureSize(textures[meta.albedoArray], 0));
    albedo.a *= 1 + max(0, calcMipLevel(fsIn.texCoords * texSize)) * mipScale;
    albedo.a = rescaleAlpha(albedo.a);

    // Ambient
    vec3 ambient = ambientLight.rgb * lightColor.rgb * albedo.rgb;

    // Diffuse
    float NdotL = dot(L, N);
    float diffuseFactor = NdotL * 0.5 + 0.5;
    diffuseFactor = diffuseFactor * diffuseFactor * fsIn.ao;
    
    // Brighten up the base of the grass blades
    float diffuseBoost = (1.0 - smoothstep(0.0, 0.2, fsIn.vertexHeight)) * diffuseBoostFactor;
    diffuseFactor += diffuseBoost;
    
    vec3 color = ambient + albedo.rgb * diffuseFactor * lightColor.rgb;
    fragColor = vec4(color, albedo.a);
}
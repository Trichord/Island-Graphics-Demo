#version 460 core

#include "binding_points.glsl"
#include "constants.glsl"
#include "textures.glsl"
#include "tone_mapping.glsl"
#include "types.glsl"
#include "foliage.glsl"
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

in VS_OUT 
{
    vec3 normal;
	vec2 texCoords;

#ifdef SHADOWS
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
    float diffuse = dot(normalizedLightDir, normalizedNormal);
    diffuse = diffuse * 0.5 + 0.5;
    diffuse = diffuse * diffuse;

	// Scale alpha by mip level 0
    vec2 texSize = vec2(textureSize(textures[meta.albedoArray], 0));
    albedo.a *= 1 + max(0, calcMipLevel(fsIn.texCoords * texSize)) * mipScale;
    albedo.a = rescaleAlpha(albedo.a);
    
    // Ambient
    vec3 ambient = ambientLight * lightColor * albedo.rgb;
    
    #ifdef SHADOWS
	// Pick shadow cascade off frag depth
	int layer = calculateCascadeIndex(-fsIn.viewPos.z);
	float shadow = ShadowContribution(lightMatrices[layer], fsIn.worldPos, layer);
    vec3 diffuseColor = diffuse * lightColor * shadow * albedo.rgb;
    
    #else
    vec3 diffuseColor = diffuse * lightColor * albedo.rgb;   
    #endif

    vec3 color = ambient + diffuseColor;

    // Tone mapping
    color = ACESFilm(color);

    // Gamma correction
	color = pow(color, vec3(1.0/gamma)); 
   
    fragColor = vec4(color, albedo.a);
}
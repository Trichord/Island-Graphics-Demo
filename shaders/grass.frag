#version 460 core

#include "binding_points.glsl"
#include "constants.glsl"
#include "textures.glsl"
#include "tone_mapping.glsl"
#include "types.glsl"
#include "foliage.glsl"

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

in vec3 normal;
in vec2 texCoords;

// Out
out vec4 fragColor;

void main() 
{
    Metadata meta = metadata[metadataIndex];

    vec4 albedo = texture(textures[meta.albedoArray], vec3(texCoords, meta.albedoLayer));

    // Flip normal if 2D quad is facing away
    vec3 normalizedNormal = normalize(normal);
    normalizedNormal = gl_FrontFacing ? normalizedNormal : -normalizedNormal;

    // Light Direction
    vec3 lightDirection = normalize(-lightDir);
    float NdotL = dot(normalizedNormal, lightDirection);

    // Diffuse
    float diffuseStrength = NdotL * 0.5 + 0.6;

	// Scale alpha by mip level 0
    vec2 texSize = vec2(textureSize(textures[meta.albedoArray], 0));
    albedo.a *= 1 + max(0, calcMipLevel(texCoords * texSize)) * mipScale;
    albedo.a = rescaleAlpha(albedo.a);
    
    // Ambient
    vec3 ambient = ambientLight * lightColor * albedo.rgb;
    
    // Diffuse continued after filling texture load with math
    vec3 diffuse = diffuseStrength * lightColor * albedo.rgb;
    vec3 color = ambient + diffuse;

    // Tone mapping
    color = ACESFilm(color);

    // Gamma correction
	color = pow(color, vec3(1.0/gamma)); 
   
    fragColor = vec4(color, albedo.a);
}
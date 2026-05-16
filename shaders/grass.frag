#version 460 core

// Types
struct Metadata
{
	int albedoArray;
	int albedoLayer;
	int armArray;
	int armLayer;
	int normalArray;
	int normalLayer;
	int cubemapArray;
	int cubemapLayer;
	uint visibleInstancesBufferBaseIndex;
};

// Constants
const float minVariance = 0.000001;
const int cascadeCount = 3;
const float lightBleedReduction = 0.2;

// Samplers
layout(binding = 0) uniform sampler2DArray textures[10];

// UBOs
layout (std140, binding = 1) uniform shadowCascadeData
{
	mat4 matrices[3];
	vec4 splits;
};

// SSBOs
layout(binding = 3, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadata[];
};

// In
uniform float gamma;
uniform float mipScale;
uniform float alphaCutoff;

uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientLight;

flat in int metadataIndex;

in vec3 normal;
in vec2 texCoords;

// Out
out vec4 fragColor;

// ACES film tone mapping (Krzysztof Narkowicz)
vec3 ACESFilm(vec3 x) 
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;

    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Credit: Ben Golus "Anti-aliased Alpha Test: The Esoteric Alpha To Coverage"
float CalcMipLevel(vec2 texture_coord)
{
    vec2 dx = dFdx(texture_coord);
    vec2 dy = dFdy(texture_coord);
    float delta_max_sqr = max(dot(dx, dx), dot(dy, dy));
                
    return max(0.0, 0.5 * log2(delta_max_sqr));
}

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
    albedo.a *= 1 + max(0, CalcMipLevel(texCoords * texSize)) * mipScale;

	// Rescale alpha by partial derivative. Credit: Ben Golus
    albedo.a = (albedo.a - alphaCutoff) / max(fwidth(albedo.a), 0.0001) + 0.5;
    
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
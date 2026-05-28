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
	uint visibleInstancesBaseIndex;
};

// Constants
const float INVERSE_PI = 0.31830988618;

// Samplers
layout(binding = 0) uniform sampler2DArray textures[11];

// UBOs
layout (std140, binding = 0) uniform cameraData 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
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

in vsOut 
{
	vec3 fragPos;
	vec3 normal;
	vec2 texCoords;
} fsIn;

// Out
out vec4 fragColor;

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
float CalcMipLevel(vec2 texture_coord){
    vec2 dx = dFdx(texture_coord);
    vec2 dy = dFdy(texture_coord);
    float delta_max_sqr = max(dot(dx, dx), dot(dy, dy));
                
    return max(0.0, 0.5 * log2(delta_max_sqr));
}

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
    albedo.a *= 1 + max(0, CalcMipLevel(fsIn.texCoords * texSize)) * mipScale;

	// Rescale alpha by partial derivative. Credit: Ben Golus
    albedo.a = (albedo.a - alphaCutoff) / max(fwidth(albedo.a), 0.0001) + 0.5;

	// Vectors
	vec3 N = normalize(normal);
	vec3 V = normalize(cameraPosition.xyz - fsIn.fragPos);
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
	vec3 diffuse = kD * albedo.rgb * INVERSE_PI * burleyFactor;

	vec3 accumulatedLight = diffuse * lightColor * NdotL;
	
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



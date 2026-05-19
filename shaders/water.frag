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
const float waveSpeed = 0.05;
const float dudvStrength = 0.01;
const float specularDamper = 0.2;

const float waterTransparency = 0.35;
const vec4 waterColor = vec4(0.01, 0.48, 0.68, 0.0) * 0.2;

// Samplers
layout(binding = 0) uniform sampler2DArray textures[10];
layout(binding = 10) uniform samplerCubeArray cubemaps[3];

// UBOs
layout (std140, binding = 0) uniform cameraData 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
	layout (offset = 272) float time;
};

// SSBOs
layout(binding = 3, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadata[];
};

// In
uniform vec3 lightDir;
uniform vec3 lightColor;

flat in float scale;
flat in int metadataIndex;

in vsOut 
{
	vec2 texCoords;
	vec3 fragPos;
	vec4 clipSpace;
} fsIn;

// Out
out vec4 fragColor;

void main() 
{	
	Metadata meta = metadata[metadataIndex];

	vec3 V = normalize(cameraPosition.xyz - fsIn.fragPos);
	vec3 L = normalize(-lightDir);
	vec3 H = normalize(L + V);

	// Move textures by time
	float movement = time * waveSpeed * 1.0 / (1.0 + log(1.0 + scale.x));

	// Repeat texture based off model scale
	float tileFactor = scale.x / 12.0;

	// Add tiling and time based movement
	vec2 d1 = fsIn.texCoords * tileFactor + vec2(movement, 0.0);
	vec2 d2 = fsIn.texCoords * tileFactor + vec2(0.0, movement);
	
	// Distortion from dudv, remapped to [-1, 1] to sample distortion in all directions
	vec2 distortion1 = texture(textures[meta.armArray], vec3(d1, meta.armLayer)).rg;
	distortion1 = (distortion1 * 2.0 - 1.0) * dudvStrength;

	vec2 distortion2 = texture(textures[meta.armArray], vec3(d2, meta.armLayer)).rg;
	distortion2 = (distortion2 * 2.0 - 1.0) * dudvStrength;
	
	// Total distortion from dudv
	vec2 totalDistortion = distortion1 + distortion2;
	vec2 normalCoords = vec2(fsIn.texCoords * tileFactor + totalDistortion);

	// Normal, biased towards the sky for less choppy water. Swizzled because the plane is horizontal
	vec3 N;
	N.xy = texture(textures[meta.normalArray], vec3(normalCoords, meta.normalLayer)).rg * 2.0 - 1.0;
	N.z = sqrt(max(1.0 - dot(N.xy, N.xy), 0.0));
	N = normalize(vec3(N.x, N.z, -N.y));

	// Specular
	float specular = pow(max(dot(N, H), 0.0), 32.0);
	vec3 specHighlight = lightColor * specular * specularDamper;

	// Fresnel
	float fresnel = 1.0 - dot(N, V);
	fresnel = pow(fresnel, 5.0);

	// Reflection 
	vec3 reflectDir = reflect(-V, N);
	vec3 reflection = texture(cubemaps[meta.cubemapArray], vec4(reflectDir, meta.cubemapLayer)).rgb;
	
	float alpha = mix(waterTransparency, 1.0, fresnel);

	fragColor = vec4(reflection, alpha) + vec4(specHighlight, 0.0) + waterColor;
}
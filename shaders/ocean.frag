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
	uint matricesBufferBaseIndex;
	uint visibleInstancesBufferBaseIndex;
};

// Constants
const float fogStart = 1142.0;
const float fogEnd = 2186.0;

const float waveSpeed = 0.01;
const float tileFactor = 100.0;
const float dudvStrength = 0.025;

const vec3 sceneCenter = vec3(0.0);
const vec3 waterColor = vec3(0.0, 0.08, 0.18);

// Samplers
layout(binding = 0) uniform sampler2DArray textures[8];
layout(binding = 8) uniform samplerCubeArray cubemaps[3];

// UBOs
layout (std140) uniform uniformData 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
	layout (offset = 272) float time;
};

// SSBOs
layout(binding = 2, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadataBuffer[];
};

// In
uniform vec3 lightDir;

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
	Metadata metadata = metadataBuffer[metadataIndex];

	vec3 V = normalize(cameraPosition.xyz - fsIn.fragPos);

	// Move textures by time
	float movement = time * waveSpeed;

	// Add tiling and time based movement
	vec2 d1 = fsIn.texCoords * tileFactor + vec2(movement, 0.0);
	vec2 d2 = fsIn.texCoords * tileFactor + vec2(0.0, movement);
	
	// Distortion from dudv, remapped to [-1, 1] to sample distortion in all directions
	vec2 distortion1 = texture(textures[metadata.armArray], vec3(d1, metadata.armLayer)).rg;
	distortion1 = (distortion1 * 2.0 - 1.0) * dudvStrength;

	vec2 distortion2 = texture(textures[metadata.armArray], vec3(d2, metadata.armLayer)).rg;
	distortion2 = (distortion2 * 2.0 - 1.0) * dudvStrength;
	
	// Total distortion from dudv
	vec2 totalDistortion = distortion1 + distortion2;

	// Normal, biased towards the sky for less choppy water. Swizzled because the plane is horizontal
	vec2 normalCoords = vec2(fsIn.texCoords * tileFactor + totalDistortion);
	vec3 N = texture(textures[metadata.normalArray], vec3(normalCoords, metadata.normalLayer)).xyz * 2.0 - 1.0;
	N = normalize(vec3(N.x, N.z, -N.y));

	// Fresnel
	float fresnel = max(0.0, min(1.0, pow(max(1 + 0.85 * dot(-V, N), 0.0), 4.0)));

	// Reflection 
	vec3 reflectDir = reflect(-V, N);
	vec3 reflection = texture(cubemaps[metadata.cubemapArray], vec4(reflectDir, metadata.cubemapLayer)).rgb;

	vec3 oceanColor = mix(waterColor, reflection, fresnel);

	// Sample absolute skybox color
	vec3 skybox = texture(cubemaps[metadata.cubemapArray], vec4(-V, metadata.cubemapLayer)).rgb;

	// Blend into distance
	float dist = length(fsIn.fragPos - sceneCenter);
	float alpha = (fogEnd - dist) / (fogEnd - fogStart);
	alpha = 1.0 - clamp(alpha, 0.0, 1.0);

	oceanColor = mix(oceanColor, skybox, alpha);

	fragColor = vec4(oceanColor, 1.0);
}
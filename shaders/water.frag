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
const float waveSpeed = 0.05;
const float dudvStrength = 0.01;
const float specularDamper = 0.2;

const float waterTransparency = 0.35;
const vec4 waterColor = vec4(0.01, 0.48, 0.68, 0.0) * 0.2;

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
	Metadata metadata = metadataBuffer[metadataIndex];

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

	// Specular
	float specular = pow(max(dot(N, H), 0.0), 32.0);
	vec3 specHighlight = lightColor * specular * specularDamper;

	// Fresnel
	float fresnel = 1.0 - dot(N, V);
	fresnel = pow(fresnel, 5.0);

	// Reflection 
	vec3 reflectDir = reflect(-V, N);
	vec3 reflection = texture(cubemaps[metadata.cubemapArray], vec4(reflectDir, metadata.cubemapLayer)).rgb;
	
	float alpha = mix(waterTransparency, 1.0, fresnel);

	fragColor = vec4(reflection, alpha) + vec4(specHighlight, 0.0) + waterColor;
}
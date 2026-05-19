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

// Samplers
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
uniform float gamma;

in flat int metadataIndex;

in vec3 normal;
in vec3 position;

// Out
out vec4 fragColor;

void main() 
{
	Metadata meta = metadata[metadataIndex];

	vec3 viewDir = normalize(position - cameraPosition.xyz);
	vec3 reflectDir = reflect(viewDir, normalize(normal));

	vec3 color = texture(cubemaps[meta.cubemapArray], vec4(reflectDir, meta.cubemapLayer)).rgb;
	color = pow(color, vec3(1.0/gamma));
	
	fragColor = vec4(color, 1.0); 
}
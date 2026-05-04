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

// Samplers
layout(binding = 8) uniform samplerCubeArray cubemaps[3];

// SSBOs
layout(binding = 2, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadataBuffer[];
};

// In
uniform float gamma;

flat in int metadataIndex;

in vec3 texCoords;

// Out
out vec4 fragColor;

void main() {
	Metadata metadata = metadataBuffer[metadataIndex];

	vec3 color = texture(cubemaps[metadata.cubemapArray], vec4(texCoords, metadata.cubemapLayer)).rgb;
	
	// Gamma correction
	color = pow(color, vec3(1.0/gamma));
	
	fragColor = vec4(color, 1.0); 
}
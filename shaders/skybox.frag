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
layout(binding = 3, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadata[];
};

// In
uniform float gamma;

flat in int metadataIndex;

in vec3 texCoords;

// Out
out vec4 fragColor;

void main() 
{
	Metadata meta = metadata[metadataIndex];

	vec3 color = texture(cubemaps[meta.cubemapArray], vec4(texCoords, meta.cubemapLayer)).rgb;
	
	// Gamma correction
	color = pow(color, vec3(1.0/gamma));
	
	fragColor = vec4(color, 1.0); 
}
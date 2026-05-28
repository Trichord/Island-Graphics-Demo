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
layout(binding = 0) uniform sampler2DArray textures[11];

// SSBOs
layout(binding = 3, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadata[];
};

// In
in vec2 texCoords;
in flat int metadataIndex;

void main()
{	
	Metadata meta = metadata[metadataIndex];
	int albedoArray = meta.albedoArray;
	int albedoLayer = meta.albedoLayer;

	float alpha = texture(textures[albedoArray], vec3(texCoords, albedoLayer)).a;

	if (alpha < 0.1)
		discard;
}
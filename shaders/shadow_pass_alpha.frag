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
const float lightBleedReduction = 0.2f;

// Samplers
layout(binding = 0) uniform sampler2DArray arrayTextures[8];

// SSBOs
layout(binding = 2, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadataBuffer[];
};

// In
in vec2 texCoords;
in flat int metadataIndex;

// Out
out vec2 fragColor;

//Credit: Andrew Lauritzen
vec2 computeMoments(float depth)
{
  vec2 moments;
  moments.x = depth;
  moments.y = depth * depth;
  return moments;
}

void main()
{	
	Metadata metadata = metadataBuffer[metadataIndex];
	int albedoArray = metadata.albedoArray;
	int albedoLayer = metadata.albedoLayer;

	float alpha = texture(arrayTextures[albedoArray], vec3(texCoords, albedoLayer)).a;

	if (alpha < 0.1)
		discard;

	fragColor = computeMoments(gl_FragCoord.z);
}
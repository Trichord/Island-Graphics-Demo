#version 460 core

layout (location = 0) in vec3 aPos;

layout (std140) uniform uniformData 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
	layout (offset = 272) float time;
};

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


layout(binding = 2, std430) readonly buffer metadataSSBO
{
	Metadata metadataBuffer[];
};

out vec3 texCoords;

flat out int cubeArray;
flat out int cubeLayer;

void main() {
	mat4 viewNoTrans = mat4(mat3(viewMatrix));
	vec4 pos = projectionMatrix * viewNoTrans * vec4(aPos, 1.0f);

	texCoords = aPos;
	cubeArray = metadataBuffer[gl_BaseInstance].cubemapArray;
	cubeLayer = metadataBuffer[gl_BaseInstance].cubemapLayer;

	gl_Position = pos.xyww;
}
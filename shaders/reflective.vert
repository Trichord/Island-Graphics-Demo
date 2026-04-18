#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNorm;

layout (std140) uniform uniformData 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
	layout (offset = 272) float time;
};

layout(binding = 0, std430) readonly buffer matricesSSBO {
    mat4 instances[];
};

layout(binding = 1, std430) readonly buffer indicesSSBO {
    int indices[];
};

struct Metadata
{
	int albedoArray;
	int albedoLayer;
	int armArray;
	int armLayer;
	int normalArray;
	int normalLayer;
	int matricesBufferBaseIndex;
};

layout(binding = 2, std430) readonly buffer metadataSSBO
{
	Metadata metadata[];
};

out vec3 normal;
out vec3 fragPos;
out vec3 position;

uniform int metadataIndex;
uniform int indicesOffset;

void main() {

	Metadata primitiveData = metadata[metadataIndex];

	int matricesIndex = primitiveData.matricesBufferBaseIndex;
	int instanceIndex = indices[indicesOffset + gl_InstanceID];

	mat4 modelMatrix = instances[matricesIndex + instanceIndex];
	mat3 normalMatrix = mat3(modelMatrix);

	normal = normalMatrix * aNorm;
	position = vec3(modelMatrix * vec4(aPos, 1.0f));
	gl_Position = projectionViewMatrix * vec4(position, 1.0f);
}
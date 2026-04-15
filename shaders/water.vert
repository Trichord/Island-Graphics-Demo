#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 3) in vec2 aTexCoords;

layout (std140, binding = 0) uniform cameraData {
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
};

layout(binding = 0, std430) readonly buffer matricesSSBO {
    mat4 instances[];
};

layout(binding = 1, std430) readonly buffer indicesSSBO {
    uint indices[];
};

out vsOut 
{
	vec2 texCoords;
	vec3 fragPos;
	vec4 clipSpace;
} vOut;

uniform uint matricesOffset;
uniform uint indicesOffset;

void main () 
{
	uint instanceIndex = indices[indicesOffset + gl_InstanceID];
	mat4 modelMatrix = instances[matricesOffset + instanceIndex];
	mat3 normalMatrix = mat3(modelMatrix);

	vOut.texCoords = aTexCoords;
	vOut.fragPos = (modelMatrix * vec4(aPos, 1.0)).xyz;
	vOut.clipSpace = projectionViewMatrix * modelMatrix * vec4(aPos, 1.0);

	gl_Position = vOut.clipSpace;
}
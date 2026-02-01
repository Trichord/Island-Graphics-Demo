#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aTangent;
layout (location = 3) in vec2 aTexCoords;

layout (std140) uniform matrices {
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionViewMatrix;
	layout (offset = 128) vec4 cameraPosition;
};

struct instanceData
{
	mat4 modelMatrix;
	mat4 normalMatrix;
};

layout(binding = 0, std430) readonly buffer ssbo 
{
    instanceData instances[];
};

out VS_OUT 
{
	vec3 fragPos;
	vec3 normal;
	vec2 texCoords;
} vs_out;

uniform int index;

void main() 
{
	mat4 modelMatrix = instances[index + gl_InstanceID].modelMatrix;
	mat3 normalMatrix = mat3(instances[index + gl_InstanceID].normalMatrix);
	vs_out.texCoords = aTexCoords;
	vs_out.fragPos = (modelMatrix * vec4(aPos, 1.0)).xyz;
	vs_out.normal = normalize(normalMatrix * aNormal);
	
	gl_Position = projectionViewMatrix * modelMatrix * vec4(aPos, 1.0);
}
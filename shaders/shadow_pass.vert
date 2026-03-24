#version 460 core

layout (location = 0) in vec3 aPos;

layout(binding = 0, std430) readonly buffer matricesSSBO 
{
    mat4 instances[];
};

layout (std140, binding = 1) uniform shadowCascadeData
{
	mat4 matrices[3];
	vec4 splits;
};

uniform uint matricesOffset;
uniform mat4 lightViewProjection;

void main()
{	
	mat4 modelMatrix = instances[matricesOffset + gl_InstanceID];
	vec4 vertexPos = lightViewProjection * modelMatrix * vec4(aPos, 1.0);

	gl_Position = vertexPos;
}
#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 3) in vec2 aTexCoords;

layout(binding = 0, std430) readonly buffer matricesSSBO 
{
    mat4 instances[];
};

layout(binding = 1, std430) readonly buffer indicesSSBO {
    uint indices[];
};

uniform uint matricesOffset;
uniform uint indicesOffset;
uniform mat4 lightViewProjection;
uniform bool useIndices;

out vec2 texCoords;

void main()
{	
	mat4 modelMatrix;

	if (useIndices)
	{
		uint instanceIndex = indices[indicesOffset + gl_InstanceID];
		modelMatrix = instances[matricesOffset + instanceIndex];
	}
	else
		modelMatrix = instances[matricesOffset + gl_InstanceID];

	vec4 vertexPos = lightViewProjection * modelMatrix * vec4(aPos, 1.0);
	texCoords = aTexCoords;

	gl_Position = vertexPos;
}
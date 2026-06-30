#version 460 core

#include "shared/binding_points.glsl"
#include "shared/types.glsl"
#include "shared/ubo.glsl"

// Attributes
layout (location = 0) in vec3 aPos;
layout (location = 3) in vec2 aTexCoords;
layout (location = 4) in uint aBoneDataIndex;

// SSBOs
layout(binding = SSBO_BINDING_MATRICES, std430) restrict readonly buffer matricesSSBO
{
   mat4 matrices[];
};

layout(binding = SSBO_BINDING_METADATA, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadata[];
};

layout(binding = SSBO_BINDING_VISIBLE_INSTANCES, std430) restrict readonly buffer visibleInstancesSSBO 
{
   uint instances[];
};

#ifdef SKINNED

layout(binding = SSBO_BINDING_BONE_TRANSFORMS, std430) restrict readonly buffer boneTransformsSSBO
{
	mat4 boneTransforms[];
};

layout(binding = SSBO_BINDING_VERTEX_BONE_DATA, std430) restrict readonly buffer vertexBoneDataSSBO
{
	VertexSkin vertexSkins[];
};

#endif

// Out
out vec2 texCoords;
flat out int array;
flat out int layer;

void main() 
{
	Metadata meta = metadata[gl_BaseInstance];
	uint matrixIndex = instances[meta.visibleInstancesBaseIndex + gl_InstanceID];

	mat4 modelMatrix = matrices[matrixIndex];

	vec4 position = vec4(aPos, 1.0);

	#ifdef SKINNED

	if (aBoneDataIndex != 0xFFFFFFFF)
	{
		VertexSkin skin = vertexSkins[aBoneDataIndex];
		vec4 acc = vec4(0.0);

		for (int i = 0; i < 4; i++)
		{	
			mat4 boneTransform = boneTransforms[skin.boneIDs[i]];
			acc += skin.weights[i] * boneTransform * position;
		}

		position = acc;
	}
	
	#endif

	texCoords = aTexCoords;
	array = meta.albedoArray;
	layer = meta.albedoLayer;

	gl_Position = viewProjectionMatrix * modelMatrix * position;
}
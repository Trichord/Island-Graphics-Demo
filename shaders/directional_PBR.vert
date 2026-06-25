#version 460 core

#include "binding_points.glsl"
#include "types.glsl"
#include "ubo.glsl"

// Attributes
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aNormal;
layout (location = 2) in vec4 aTangent;
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

// In
uniform vec3 lightDir;
uniform mat4 lightViewProjection;

// Out
flat out int metadataIndex;

out vsOut 
{
	vec3 cameraPos;
	vec3 lightDir;
	vec3 tangentFragPos;
	vec2 texCoords;
	vec4 viewFragPos;
	vec4 worldFragPos;
} vOut;

void main() 
{
	Metadata meta = metadata[gl_BaseInstance];
	uint matrixIndex = instances[meta.visibleInstancesBaseIndex + gl_InstanceID];

	mat4 modelMatrix = matrices[matrixIndex];
	mat3 normalMatrix = mat3(modelMatrix);

	//TBN matrix for normal map
	vec3 T = normalize(normalMatrix * aTangent.xyz);
	vec3 N = normalize(normalMatrix * aNormal.xyz);
	T = normalize(T - dot(T, N) * N); 
	vec3 B = normalize(cross(N, T) * aTangent.w);
	mat3 TBN = transpose(mat3(T, B, N));

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

	vOut.worldFragPos = modelMatrix * position;
	vOut.viewFragPos = viewMatrix * vOut.worldFragPos;
	vOut.tangentFragPos = TBN * vOut.worldFragPos.xyz;

	vOut.lightDir = TBN * lightDir;
	vOut.cameraPos = TBN * cameraPosition.xyz;

	vOut.texCoords = aTexCoords;

	metadataIndex = gl_BaseInstance;
	
	gl_Position = viewProjectionMatrix * vOut.worldFragPos;
}
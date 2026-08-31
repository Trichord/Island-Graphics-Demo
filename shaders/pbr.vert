#version 460 core

#include "shared/binding_points.glsl"
#include "shared/types.glsl"
#include "shared/ubo.glsl"

// Attributes
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aNormal;
layout (location = 2) in vec4 aTangent;
layout (location = 3) in vec2 aTexCoords;

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

// Out
flat out int metadataIndex;

out vsOut 
{
	vec4 worldFragPos;
	vec3 tangentCameraPos;
	vec3 tangentLightDir;
	vec3 tangentFragPos;
	vec3 geoNormal;
	vec2 texCoords;
	float viewFragDepth;
} vOut;

void main() 
{
	Metadata meta = metadata[gl_BaseInstance];
	uint matrixIndex = instances[meta.visibleInstancesBaseIndex + gl_InstanceID];

	mat4 modelMatrix = matrices[matrixIndex];
	mat3 normalMatrix = mat3(modelMatrix);
	vOut.geoNormal = normalMatrix * aNormal.xyz;

	//TBN matrix for normal map
	vec3 T = normalize(normalMatrix * aTangent.xyz);
	vec3 N = normalize(vOut.geoNormal);
	T = normalize(T - dot(T, N) * N); 
	vec3 B = cross(N, T) * aTangent.w;
	mat3 TBN = transpose(mat3(T, B, N));

	vOut.worldFragPos = modelMatrix * vec4(aPos, 1.0);
	vOut.viewFragDepth = (viewMatrix * vOut.worldFragPos).z;

	vOut.tangentFragPos = TBN * vOut.worldFragPos.xyz;
	vOut.tangentLightDir = TBN * lightDir.xyz;
	vOut.tangentCameraPos = TBN * cameraPosition.xyz;

	vOut.texCoords = aTexCoords;

	metadataIndex = gl_BaseInstance;
	
	gl_Position = viewProjectionMatrix * vOut.worldFragPos;
}
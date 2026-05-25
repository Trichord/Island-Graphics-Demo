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
	uint visibleInstancesBaseIndex;
};

// Attributes
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aTangent;
layout (location = 3) in vec2 aTexCoords;

// UBOs
layout (std140, binding = 0) uniform cameraData 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
};

// SSBOs
layout(binding = 2, std430) restrict readonly buffer matricesSSBO 
{
   mat4 matrices[];
};

layout(binding = 3, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadata[];
};

layout(binding = 7, std430) restrict readonly buffer visibleInstancesSSBO 
{
   uint instances[];
};

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
	vec3 N = normalize(normalMatrix * aNormal);
	T = normalize(T - dot(T, N) * N); 
	vec3 B = normalize(cross(N, T) * aTangent.w);
	mat3 TBN = transpose(mat3(T, B, N));

	vOut.worldFragPos = modelMatrix * vec4(aPos, 1.0);
	vOut.viewFragPos = viewMatrix * vOut.worldFragPos;
	vOut.tangentFragPos = TBN * vOut.worldFragPos.xyz;

	vOut.lightDir = TBN * lightDir;
	vOut.cameraPos = TBN * cameraPosition.xyz;

	vOut.texCoords = aTexCoords;

	metadataIndex = gl_BaseInstance;
	
	gl_Position = projectionViewMatrix * vOut.worldFragPos;
}
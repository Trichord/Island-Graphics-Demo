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
	uint visibleInstancesBufferBaseIndex;
};

// Attributes
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aTangent;
layout (location = 3) in vec2 aTexCoords;

// UBOs
layout (std140) uniform cameraData 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
};

// SSBOs
layout(binding = 0, std430) restrict readonly buffer matricesSSBO {
    mat4 instances[];
};

layout(binding = 1, std430) restrict readonly buffer indicesSSBO {
    uint indices[];
};

layout(binding = 2, std430) restrict readonly buffer metadataSSBO
{
	Metadata metadataBuffer[];
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
} vOut;

void main () 
{
	Metadata metadata = metadataBuffer[gl_BaseInstance];
	uint matrixIndex = indices[metadata.visibleInstancesBufferBaseIndex + gl_InstanceID];

	mat4 modelMatrix = instances[matrixIndex];
	mat3 normalMatrix = mat3(modelMatrix);

	// TBN matrix for normal map
	vec3 T = normalize(normalMatrix * aTangent.xyz);
	vec3 N = normalize(normalMatrix * aNormal);
	T = normalize(T - dot(T, N) * N); 
	vec3 B = normalize(cross(N, T) * aTangent.w);
	mat3 TBN = transpose(mat3(T, B, N));

	vec4 worldPos = modelMatrix * vec4(aPos, 1.0);

	metadataIndex = gl_BaseInstance;

	vOut.tangentFragPos = TBN * worldPos.xyz;
	vOut.lightDir = TBN * lightDir;
	vOut.cameraPos = TBN * cameraPosition.xyz;
	vOut.texCoords = aTexCoords;

	gl_Position = projectionViewMatrix * worldPos;
}
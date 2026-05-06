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

// Constants
const float swaySpeed = 0.75;
const float minVertexHeight = 0.1;
const float maxVertexHeight = 25.0;
const float PI = 3.14159265358979323846;

// Attributes
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aTangent;
layout (location = 3) in vec2 aTexCoords;

// UBOs
layout (std140) uniform uniformData 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
	layout (offset = 272) float time;
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
uniform float windStrength;
uniform vec3 windDirection;

// Out
flat out int metadataIndex;

out vsOut 
{
	vec3 fragPos;
	vec3 normal;
	vec2 texCoords;
} vOut;

// Pseudo-random number generator
float random(vec2 st)
{
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

vec3 calcWindTranslation(mat4 transform, float timeStamp, vec3 windDir, float windStr)
{
	const vec4 translation = transform[3];
	float wave = sin(timeStamp + random(vec2(translation.x, translation.z)));
	return windDir * wave * windStr;
}

void main() 
{
	Metadata meta = metadata[gl_BaseInstance];
	uint matrixIndex = instances[meta.visibleInstancesBufferBaseIndex + gl_InstanceID];

	mat4 modelMatrix = matrices[matrixIndex];
	mat3 normalMatrix = mat3(modelMatrix);

	vec3 windTranslation = calcWindTranslation(modelMatrix, time * swaySpeed, normalize(windDirection), windStrength);
	float mask = smoothstep(minVertexHeight, maxVertexHeight, aPos.y);
	windTranslation *= mask;

	vec4 worldPos = modelMatrix * vec4(aPos + windTranslation, 1.0);

	metadataIndex = gl_BaseInstance;

	vOut.fragPos = worldPos.xyz;
	vOut.texCoords = aTexCoords;
	vOut.normal = normalize(normalMatrix * aNormal);

	gl_Position = projectionViewMatrix * worldPos;
}
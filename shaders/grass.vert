#version 460 core

#include "binding_points.glsl"
#include "types.glsl"
#include "ubo.glsl"
#include "wind.glsl"

// Constants
const float swaySpeed = 0.75;
const float maxVertexHeight = 1.0;
const float minVertexHeight = 0.001;
const float windScale = 0.1;
const vec3 worldUp = vec3(0.0, 1.0, 0.0);

// Attributes
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
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

// In
uniform float windStrength;
uniform vec3 windDirection;

// Out
flat out int metadataIndex;

out vec3 normal;
out vec2 texCoords;

#ifdef SHADOWS
out vec4 viewPos;
out vec4 worldPos;
#endif

void main() 
{
	Metadata meta = metadata[gl_BaseInstance];
	uint matrixIndex = instances[meta.visibleInstancesBaseIndex + gl_InstanceID];

	mat4 modelMatrix = matrices[matrixIndex];
	mat3 normalMatrix = mat3(modelMatrix);
	vec4 worldPosition = modelMatrix * vec4(aPos, 1.0);

	vec3 windTranslation = calcWindTranslation(modelMatrix, time * swaySpeed, windDirection, windStrength);
	float mask = smoothstep(minVertexHeight, maxVertexHeight, aPos.y);
	windTranslation *= mask;
	windTranslation *= windScale;

	metadataIndex = gl_BaseInstance;
	texCoords = aTexCoords;
	normal = normalize(normalMatrix * mix(aNormal, worldUp, 0.5));

	#ifdef SHADOWS
	viewPos = viewMatrix * worldPosition;
	worldPos = worldPosition;
	#endif

    gl_Position = projectionViewMatrix * vec4(worldPosition.xyz + windTranslation, 1.0);
}
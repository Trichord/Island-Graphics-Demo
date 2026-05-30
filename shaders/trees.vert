#version 460 core

#include "binding_points.glsl"
#include "types.glsl"
#include "ubo.glsl"
#include "wind.glsl"

// Constants
const float swaySpeed = 0.75;
const float minVertexHeight = 0.1;
const float maxVertexHeight = 25.0;

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

out vsOut 
{
	vec4 worldPos;
	vec3 normal;
	vec2 texCoords;

	#ifdef SHADOWS
	vec4 viewPos;
	#endif

} vOut;

void main() 
{
	Metadata meta = metadata[gl_BaseInstance];
	uint matrixIndex = instances[meta.visibleInstancesBaseIndex + gl_InstanceID];

	mat4 modelMatrix = matrices[matrixIndex];
	mat3 normalMatrix = mat3(modelMatrix);

	vec3 windTranslation = calcWindTranslation(modelMatrix, time * swaySpeed, normalize(windDirection), windStrength);
	float mask = smoothstep(minVertexHeight, maxVertexHeight, aPos.y);
	windTranslation *= mask;

	vec4 worldPos = modelMatrix * vec4(aPos + windTranslation, 1.0);

	metadataIndex = gl_BaseInstance;

	vOut.worldPos = worldPos;
	vOut.texCoords = aTexCoords;
	vOut.normal = normalize(normalMatrix * aNormal);

	#ifdef SHADOWS
	vOut.viewPos = viewMatrix * worldPos;
	#endif

	gl_Position = projectionViewMatrix * worldPos;
}
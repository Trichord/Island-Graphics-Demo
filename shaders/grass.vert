#version 460 core

#include "shared/binding_points.glsl"
#include "shared/textures.glsl"
#include "shared/types.glsl"
#include "shared/ubo.glsl"
#include "shared/wind.glsl"

// Constants
const vec3 worldUp = vec3(0.0, 1.0, 0.0);

// Attributes
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aNormal;
layout (location = 2) in vec4 aTangent;
layout (location = 3) in vec2 aTexCoords;
layout (location = 5) in vec4 aColor;

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

out VS_OUT 
{
	vec3 normal;
	vec2 texCoords;
	float vertexHeight;

#ifdef ALPHA_TESTED_SHADOWS
	vec4 viewPos;
	vec4 worldPos;
#endif
} vsOut;

void main() 
{
	Metadata meta = metadata[gl_BaseInstance];
	uint matrixIndex = instances[meta.visibleInstancesBaseIndex + gl_InstanceID];

	mat4 modelMatrix = matrices[matrixIndex];
	mat3 normalMatrix = mat3(modelMatrix);
	vec3 normal = normalize(normalMatrix * mix(aNormal.xyz, worldUp, 0.5));
	vec4 worldPosition = modelMatrix * vec4(aPos, 1.0);
	
	// Wind
	vec2 noise = getNoise(textures[meta.normalArray], meta.normalLayer, worldPosition.xyz, time);
	float windFactor = getWindFactor(worldPosition.xyz, time);

	largeScaleWind(worldPosition.xyz, aColor, windDirection, windFactor, noise.x);
	mediumScaleWind(worldPosition.xyz, aColor, vec3(noise.rg, -noise.r), windFactor);
	smallScaleWind(worldPosition.xyz, aColor, normal, vec3(noise.gr, noise.g), aPos.y, windFactor, windStrength);
	
	// Out params
	metadataIndex = gl_BaseInstance;

	vsOut.vertexHeight = aPos.y;	
	vsOut.texCoords = aTexCoords;
	vsOut.normal = normal;
	#ifdef ALPHA_TESTED_SHADOWS
	vsOut.viewPos = viewMatrix * worldPosition;
	vsOut.worldPos = worldPosition;
	#endif

    gl_Position = viewProjectionMatrix * worldPosition;
}
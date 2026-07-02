#version 460 core

#include "shared/binding_points.glsl"
#include "shared/textures.glsl"
#include "shared/types.glsl"
#include "shared/ubo.glsl"
#include "shared/wind.glsl"

// Attributes
layout (location = 0) in vec4 aPos;

#ifdef ALPHA_TESTED_SHADOWS
layout (location = 1) in vec4 aNormal;
layout (location = 2) in vec4 aTangent;
layout (location = 3) in vec2 aTexCoords;
layout (location = 5) in vec4 aColor;
#endif

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
uniform int cascade;

#ifdef ALPHA_TESTED_SHADOWS
uniform float windStrength;
uniform vec3 windDirection;
uniform vec3 lightDir;

// Out
out vec2 texCoords;

flat out int metadataIndex;
#endif

void main()
{	
	Metadata meta = metadata[gl_BaseInstance];
	uint matrixIndex = instances[meta.visibleInstancesBaseIndex + gl_InstanceID];

	mat4 modelMatrix = matrices[matrixIndex];
	vec4 worldPosition = modelMatrix * aPos;
	
#ifdef ALPHA_TESTED_SHADOWS
	mat3 normalMatrix = mat3(modelMatrix);
	vec3 normal = normalize(normalMatrix * aNormal.xyz);
	
	// Wind
	vec2 noise = getNoise(textures[meta.normalArray], meta.normalLayer, worldPosition.xyz, time);
	float windFactor = getWindFactor(worldPosition.xyz, time);

	largeScaleWind(worldPosition.xyz, aColor, windDirection, windFactor, noise.x);
	mediumScaleWind(worldPosition.xyz, aColor, vec3(noise.rg, -noise.r), windFactor);
	smallScaleWind(worldPosition.xyz, aColor, normal, vec3(noise.gr, noise.g), aPos.y, windFactor, windStrength);
	
	
	float NdotL = dot(normal, lightDir);
	float normalBias = max(0.002 * (1.0 - NdotL), 0.005);
	
	if (NdotL < 0.0)
		worldPosition.xyz += -normal * normalBias;
	else
		worldPosition.xyz += normal * normalBias;

	texCoords = aTexCoords;
	metadataIndex = gl_BaseInstance;
#endif

	gl_Position = lightMatrices[cascade] * worldPosition;
}
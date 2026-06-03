#version 460 core

#include "binding_points.glsl"
#include "textures.glsl"
#include "types.glsl"
#include "ubo.glsl"
#include "wind.glsl"

// Attributes
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec4 aColor;

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
uniform float windStrength;
uniform vec3 windDirection;

#ifdef ALPHA_TESTED
// Out
out vec2 texCoords;

flat out int metadataIndex;
#endif

void main()
{	
	Metadata meta = metadata[gl_BaseInstance];
	uint matrixIndex = instances[meta.visibleInstancesBaseIndex + gl_InstanceID];

	mat4 modelMatrix = matrices[matrixIndex];
	
	mat3 normalMatrix = mat3(modelMatrix);
	vec3 normal = normalize(normalMatrix * aNormal);
	vec4 worldPosition = modelMatrix * vec4(aPos, 1.0);
	
	// Wind
	vec2 noise = getNoise(textures[meta.normalArray], meta.normalLayer, worldPosition.xyz, time);
	float windFactor = getWindFactor(worldPosition.xyz, time);

	largeScaleWind(worldPosition.xyz, aColor, windDirection, windFactor, noise.x);
	mediumScaleWind(worldPosition.xyz, aColor, vec3(noise.rg, -noise.r), windFactor);
	smallScaleWind(worldPosition.xyz, aColor, normal, vec3(noise.gr, noise.g), aPos.y, windFactor, windStrength);

	#ifdef ALPHA_TESTED
	texCoords = aTexCoords;
	metadataIndex = gl_BaseInstance;
	#endif

	gl_Position = lightMatrices[cascade] * worldPosition;
}
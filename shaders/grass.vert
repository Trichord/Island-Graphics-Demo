#version 460 core

#include "binding_points.glsl"
#include "textures.glsl"
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
layout (location = 4) in vec4 aColor;

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

#ifdef SHADOWS
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
	vec4 worldPos = modelMatrix * vec4(aPos, 1.0);
	vec3 normal = normalize(normalMatrix * mix(aNormal, worldUp, 0.5));
	
	// Wind
	vec2 noise = texture(textures[meta.normalArray], vec3(worldPos.xy + time * 0.05, meta.normalLayer)).rg;
	float windFactor = getWindFactor(worldPos.xyz, time);

	largeScaleWind(worldPos.xyz, aColor, windDirection, windFactor, noise.x);
	mediumScaleWind(worldPos.xyz, aColor, vec3(noise.rg, -noise.r), windFactor);
	smallScaleWind(worldPos.xyz, aColor, normal, vec3(noise.gr, noise.g), windFactor, windStrength);
	
	// Out params
	metadataIndex = gl_BaseInstance;
	
	vsOut.texCoords = aTexCoords;
	vsOut.normal = normal;
	#ifdef SHADOWS
	vsOut.viewPos = viewMatrix * worldPos;
	vsOut.worldPos = worldPos;
	#endif

    gl_Position = projectionViewMatrix * worldPos;
}
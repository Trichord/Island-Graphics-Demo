#version 460 core

out vec4 fragColor;

layout (std140) uniform matrices 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionViewMatrix;
	layout (offset = 128) vec4 cameraPosition;
};

uniform sampler2D armMap; //dudv map here
uniform sampler2D normalMap;
uniform sampler2D reflectionMap;
uniform sampler2D refractionMap;
uniform sampler2D depthMap;
uniform vec3 lightColor;
uniform vec3 lightDir;
uniform float time;

in VS_OUT 
{
	vec2 texCoords;
	vec3 fragPos;
	vec4 clipSpace;
} fs_in;

const float tileFactor = 6.0;
const float dudvStrength = 0.0075;
const float waveSpeed = 0.01;

void main() 
{
	// Move textures by time
	float movement = fract(time * waveSpeed);

	// dudv in [0 - 1], need it in [-1, 1]
	vec2 dudv = texture(armMap, fs_in.texCoords + movement).rg * 2.0 - 1.0;
	dudv *= dudvStrength;

	// Projective texture mapping
	vec2 ndc = fs_in.clipSpace.xy / fs_in.clipSpace.w;
	vec2 ndcTexCoords = ndc / 2.0 + 0.5;

	vec4 normalMap = texture(normalMap, fs_in.texCoords + dudv);

	// Reflection
	vec2 reflectTexCoords = vec2(ndcTexCoords.x, 1.0 - ndcTexCoords.y);
	reflectTexCoords += dudv;
	reflectTexCoords = clamp(reflectTexCoords, 0.001, 0.999);
	
	 // Refraction
	vec2 refractTexCoords = vec2(ndcTexCoords.x, ndcTexCoords.y);
	refractTexCoords += dudv;
	refractTexCoords = clamp(refractTexCoords, 0.001, 0.999);

	vec4 reflection = texture(reflectionMap, reflectTexCoords);
	vec4 refraction = texture(refractionMap, refractTexCoords);

	fragColor = vec4(mix(reflection, refraction, 0.5));
}
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
uniform float gamma;

in VS_OUT 
{
	vec2 texCoords;
	vec3 fragPos;
	vec4 clipSpace;
} fs_in;

const float tileFactor = 6.0;

void main() 
{
	// Projective texture mapping
	vec2 ndc = fs_in.clipSpace.xy / fs_in.clipSpace.w;
	vec2 ndcTexCoords = ndc / 2.0 + 0.5;

	vec2 reflectTexCoords = vec2(ndcTexCoords.x, 1.0 - ndcTexCoords.y);
	vec2 refractTexCoords = vec2(ndcTexCoords.x, ndcTexCoords.y);

	
	// dudv in [0 - 1], need it in [-1, 1]
	vec2 dudv = texture(armMap, fs_in.texCoords).rg * 2.0 - 1.0;
	vec4 normalMap = texture(normalMap, fs_in.texCoords + dudv);
	vec4 reflection = texture(reflectionMap, reflectTexCoords);
	vec4 refraction = texture(refractionMap, refractTexCoords);

	fragColor = vec4(mix(reflection, refraction, 0.5));
}
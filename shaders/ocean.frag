#version 460 core

out vec4 fragColor;

layout (std140) uniform uniformData 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
	layout (offset = 272) float time;
};

layout(binding = 0) uniform sampler2DArray arrayTextures[8];
layout(binding = 8) uniform samplerCube cubeMap;

//dudv in the arm slot
flat in int armArray;
flat in int armLayer;

flat in int normalArray;
flat in int normalLayer;

uniform vec3 lightDir;

uniform vec3 scale;

in vsOut 
{
	vec2 texCoords;
	vec3 fragPos;
	vec4 clipSpace;
} fsIn;

const float dudvStrength = 0.025;
const float waveSpeed = 0.01;
const float tileFactor = 100.0;
const float fogStart = 1142.0;
const float fogEnd = 2186.0;
const vec3 waterColor = vec3(0.0, 0.08, 0.18);
const vec3 sceneCenter = vec3(0.0);

void main() 
{	
	vec3 V = normalize(cameraPosition.xyz - fsIn.fragPos);

	// Move textures by time
	float movement = time * waveSpeed;

	// Add tiling and time based movement
	vec2 d1 = fsIn.texCoords * tileFactor + vec2(movement, 0.0);
	vec2 d2 = fsIn.texCoords * tileFactor + vec2(0.0, movement);
	
	// Distortion from dudv, remapped to [-1, 1] to sample distortion in all directions
	vec2 distortion1 = texture(arrayTextures[armArray], vec3(d1, armLayer)).rg;
	distortion1 = (distortion1 * 2.0 - 1.0) * dudvStrength;

	vec2 distortion2 = texture(arrayTextures[armArray], vec3(d2, armLayer)).rg;
	distortion2 = (distortion2 * 2.0 - 1.0) * dudvStrength;
	
	//Total distortion from dudv
	vec2 totalDistortion = distortion1 + distortion2;

	// Normal, biased towards the sky for less choppy water. Swizzled because the plane is horizontal
	vec2 normalCoords = vec2(fsIn.texCoords * tileFactor + totalDistortion);
	vec3 N = texture(arrayTextures[normalArray], vec3(normalCoords, normalLayer)).xyz * 2.0 - 1.0;
	N = normalize(vec3(N.x, N.z, -N.y));

	// Fresnel
	float fresnel = max(0.0, min(1.0, pow(max(1 + 0.85 * dot(-V, N), 0.0), 4.0)));

	// Reflection 
	vec3 reflectDir = reflect(-V, N);
	vec3 reflection = texture(cubeMap, reflectDir).rgb;

	vec3 oceanColor = mix(waterColor, reflection, fresnel);

	//Sample absolute skybox color
	vec3 skybox = texture(cubeMap, -V).rgb;

	//Blend into distance
	float dist = length(fsIn.fragPos - sceneCenter);
	float alpha = (fogEnd - dist) / (fogEnd - fogStart);
	alpha = 1.0 - clamp(alpha, 0.0, 1.0);

	oceanColor = mix(oceanColor, skybox, alpha);

	fragColor = vec4(oceanColor, 1.0);
}
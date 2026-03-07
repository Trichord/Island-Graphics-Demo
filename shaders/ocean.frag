#version 460 core

out vec4 fragColor;

layout (std140) uniform matrices {
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
};

uniform sampler2D armMap; //dudv map here
uniform sampler2D normalMap;
uniform samplerCube cubeMap;
uniform vec3 lightColor;
uniform vec3 lightDir;
uniform vec3 scale;
uniform float time;

in VS_OUT 
{
	vec2 texCoords;
	vec3 fragPos;
	vec4 clipSpace;
} fs_in;

const float dudvStrength = 0.025;
const float waveSpeed = 0.01;
const float specularDamper = 0.2;
const float tileFactor = 50.0;
const float fogStart = 400.0;
const float fogEnd = 900.0;
const vec3 waterColor = vec3(0.01, 0.48, 0.88) * 0.1;
const vec3 sceneCenter = vec3(0.0);

void main() 
{	
	vec3 V = normalize(cameraPosition.xyz - fs_in.fragPos);
	vec3 L = normalize(-lightDir);
	vec3 H = normalize(L + V);

	// Move textures by time
	float movement = time * waveSpeed;

	// Add tiling and time based movement
	vec2 d1 = fs_in.texCoords * tileFactor + vec2(movement, 0.0);
	vec2 d2 = fs_in.texCoords * tileFactor + vec2(0.0, movement);
	
	// Distortion from dudv, remapped to [-1, 1] to sample distortion in all directions
	vec2 distortion1 = (texture(armMap, d1).rg * 2.0 - 1.0) * dudvStrength;	
	vec2 distortion2 = (texture(armMap, d2).xy * 2.0 - 1.0) * dudvStrength;
	
	//Total distortion from dudv
	vec2 totalDistortion = distortion1 + distortion2;

	// Normal, biased towards the sky for less choppy water. Swizzled because the plane is horizontal
	vec2 normalCoords = vec2(fs_in.texCoords * tileFactor + totalDistortion);
	vec3 N = texture(normalMap, normalCoords).xyz * 2.0 - 1.0;
	N = normalize(vec3(N.x, N.z, -N.y));

	// Specular
	float specular = pow(max(dot(N, H), 0.0), 32.0);
	vec3 specHighlight = lightColor * specular * specularDamper;

	// Fresnel
	float fresnel = 1.0 - dot(N, V);
	fresnel = pow(fresnel, 5.0);

	// Reflection 
	vec3 reflectDir = reflect(-V, N);
	vec3 reflection = texture(cubeMap, reflectDir).rgb;

	//Sample absolute skybox color
	vec3 skybox = texture(cubeMap, -V).rgb;

	//Blend into distance
	float dist = length(fs_in.fragPos - sceneCenter);
	//float alpha = smoothstep(fogStart, fogEnd, dist);
	float alpha = (fogEnd - dist) / (fogEnd - fogStart);
	alpha = clamp(alpha, 0.0, 1.0);

	vec3 oceanColor = reflection + specHighlight + waterColor;
	oceanColor = mix(oceanColor, skybox, alpha);

	fragColor = vec4(oceanColor, 1.0);
}
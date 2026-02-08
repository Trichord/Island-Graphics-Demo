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

const float dudvStrength = 0.005;
const float waveSpeed = 0.002;
const float waterTransparency = 0.5;
const float specularDamper = 0.3;
const vec4 waterColor = vec4(0.0, 0.08, 0.2, 0.0);

void main() 
{	
	vec3 V = normalize(cameraPosition.xyz - fs_in.fragPos);
	vec3 L = normalize(-lightDir);
	vec3 H = normalize(L + V);

	// Move textures by time
	float movement = fract(time * waveSpeed);

	//Repeat texture based off model scale
	float tileFactor = scale.x / 10.0;

	// Distortion from dudv
	vec2 d1 = vec2(fs_in.texCoords.x + movement, fs_in.texCoords.y);
	vec2 distortedTexCoords = texture(armMap, d1 * tileFactor).rg * dudvStrength;
	distortedTexCoords = fs_in.texCoords + vec2(distortedTexCoords.x, distortedTexCoords.y + movement);

	// Normal, biased towards the sky for less choppy water. Swizzled because the plane is horizontal
	vec3 N = texture(normalMap, distortedTexCoords * tileFactor).xyz * 2.0 - 1.0;
	N = normalize(vec3(N.x, N.z * 1.5, -N.y));

	// Mix the normal back towards the flat plane
	N = mix(N, vec3(0.0, 1.0, 0.0), 0.75);

	// Specular
	float specular = pow(max(dot(N, H), 0.0), 32.0);
	vec3 specHighlight = lightColor * specular * specularDamper;

	// Fresnel
	float fresnel = 1.0 - dot(N, V);

	// Reflection 
	vec3 reflectDir = reflect(-V, N);
	vec3 reflection = texture(cubeMap, reflectDir).rgb;

	fragColor = vec4(reflection * fresnel, waterTransparency) + vec4(specHighlight, 0.0) + waterColor;
}
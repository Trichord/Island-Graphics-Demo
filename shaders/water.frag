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
uniform float time;

in VS_OUT 
{
	vec2 texCoords;
	vec3 fragPos;
	vec4 clipSpace;
} fs_in;

const float tileFactor = 4.0;
const float dudvStrength = 0.005;
const float waveSpeed = 0.002;
const float waterTransparency = 0.5;
const float specularDamper = 0.6;
const float shorelineFade = 5.0;
const vec4 waterColor = vec4(0.0, 0.075, 0.2, 0.0);

const vec3 boxSize = vec3(40.0);
const vec3 worldPos = vec3(120.00715f, -4.947438f, 130.93424f);

//Fresnel-Schlick approximation (unrolled x^5)
float fresnelSchlick(float cosTheta, float F0){
	float x = clamp(1.0 - cosTheta, 0.0, 1.0);
	float x2 = x * x;
	float x4 = x2 * x2;
	float x5 = x4 * x;

	return F0 + (1.0 - F0) * x5;
}

void main() 
{	
	vec3 V = normalize(cameraPosition.xyz - fs_in.fragPos);
	vec3 L = normalize(-lightDir);
	vec3 H = normalize(L + V);

	// Move textures by time
	float movement = fract(time * waveSpeed);

	// Projective texture mapping
	vec2 ndc = fs_in.clipSpace.xy / fs_in.clipSpace.w;
	vec2 ndcTexCoords = ndc / 2.0 + 0.5;

	// Distortion from dudv
	vec2 d1 = vec2(fs_in.texCoords.x + movement, fs_in.texCoords.y);
	vec2 distortedTexCoords = texture(armMap, d1 * tileFactor).rg * dudvStrength;

	// Feed it back into the dudv again. Dudv in [0 - 1], need it in [-1, 1]
	distortedTexCoords = fs_in.texCoords + vec2(distortedTexCoords.x, distortedTexCoords.y + movement);
	vec2 totalDistortion = (texture(armMap, distortedTexCoords * tileFactor).rg * 2.0 - 1.0) * dudvStrength;

	// Normal, biased towards the sky for less choppy water
	vec3 normal = texture(normalMap, distortedTexCoords * tileFactor).xyz;
	normal.y *= 1.5; 
	vec3 N = normalize(normal * 2.0 - 1.0);
	
	// Swizzle because the plane is horizontal not vertical
	N = vec3(N.x, N.z, -N.y);

	// Specular
	float specular = pow(max(dot(N, H), 0.0), 32.0);
	vec3 specHighlight = lightColor * specular * specularDamper;

	float fresnel = fresnelSchlick(dot(N, V), 0.02);

	// Reflection 
	vec3 reflectDir = reflect(-V, vec3(0.0, 1.0, 0.0));

	// Parallax Correction
	vec3 boxMax = worldPos + boxSize;
	vec3 boxMin = worldPos - boxSize;
	
	vec3 firstPlaneIntersect = (boxMax - fs_in.fragPos) / reflectDir;
	vec3 secondPlaneIntersect = (boxMin - fs_in.fragPos) / reflectDir;
	vec3 furthestPlane = max(firstPlaneIntersect, secondPlaneIntersect);
	
	float distance = min(min(furthestPlane.x, furthestPlane.y), furthestPlane.z);
	
	vec3 intersectPosition = fs_in.fragPos + reflectDir * distance;
	
	reflectDir = intersectPosition - worldPos;
	vec3 reflection = texture(cubeMap, reflectDir).rgb;

	fragColor = vec4(reflection * fresnel, waterTransparency) + vec4(specHighlight, 0.0) + waterColor;
}
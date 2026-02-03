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
uniform float nearPlane;
uniform float farPlane;
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
	// Move textures by time
	float movement = fract(time * waveSpeed);

	// Projective texture mapping
	vec2 ndc = fs_in.clipSpace.xy / fs_in.clipSpace.w;
	vec2 ndcTexCoords = ndc / 2.0 + 0.5;

	// Reflection
	vec2 reflectTexCoords = vec2(ndcTexCoords.x, 1.0 - ndcTexCoords.y);

	// Refraction
	vec2 refractTexCoords = vec2(ndcTexCoords);

	// Depth
	float depthMap = texture(depthMap, refractTexCoords).r;
	float groundDistance = 2.0 * nearPlane * farPlane / (farPlane + nearPlane - (2.0 * depthMap - 1.0) * (farPlane - nearPlane));

	float waterDistance = gl_FragCoord.z;
	waterDistance = 2.0 * nearPlane * farPlane / (farPlane + nearPlane - (2.0 * waterDistance - 1.0) * (farPlane - nearPlane));

	float waterDepth = groundDistance - waterDistance;
	waterDepth = clamp(waterDepth / shorelineFade, 0.0, 1.0);

	// Distortion from dudv
	vec2 d1 = vec2(fs_in.texCoords.x + movement, fs_in.texCoords.y);
	vec2 distortedTexCoords = texture(armMap, d1 * tileFactor).rg * dudvStrength;

	// Feed it back into the dudv again. Dudv in [0 - 1], need it in [-1, 1]
	distortedTexCoords = fs_in.texCoords + vec2(distortedTexCoords.x, distortedTexCoords.y + movement);
	vec2 totalDistortion = (texture(armMap, distortedTexCoords * tileFactor).rg * 2.0 - 1.0) * dudvStrength * waterDepth;

	reflectTexCoords += totalDistortion;
	reflectTexCoords = clamp(reflectTexCoords, 0.0, 1.0);

	refractTexCoords += totalDistortion;
	refractTexCoords = clamp(refractTexCoords, 0.0, 1.0);

	// Normal
	vec3 normalMap = texture(normalMap, distortedTexCoords * tileFactor).xyz;
	//Bias towards the sky for less choppy water
	normalMap.y *= 3.0; 
	vec3 N = normalize(normalMap * 2.0 - 1.0);

	// Swizzle because the plane is horizontal not vertical
	N = vec3(N.x, N.z, -N.y);

	// Specular
	vec3 V = normalize(cameraPosition.xyz - fs_in.fragPos);
	vec3 L = normalize(-lightDir);
	vec3 H = normalize(L + V);
	float specular = pow(max(dot(N, H), 0.0), 32.0);
	vec3 specHighlight = lightColor * specular * specularDamper * waterDepth;

	vec3 reflection = texture(reflectionMap, reflectTexCoords).rgb;
	vec3 refraction = texture(refractionMap, refractTexCoords).rgb;

	float fresnel = fresnelSchlick(dot(N, V), 0.02);

	// Right before your final fragColor line, add:
if (reflectTexCoords.x < 0.0 || reflectTexCoords.x > 1.0 || 
    reflectTexCoords.y < 0.0 || reflectTexCoords.y > 1.0) {
    fragColor = vec4(1.0, 0.0, 1.0, 1.0); // Magenta
    return;
}


	fragColor = vec4(mix(reflection, refraction, fresnel), waterDepth) + vec4(specHighlight, 0.0);
	fragColor.a = waterTransparency;
}
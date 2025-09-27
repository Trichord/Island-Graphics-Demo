#version 330 core

out vec4 fragColor;

uniform sampler2D albedoMap;
uniform sampler2D occlusionRoughMetalMap;
uniform sampler2D normalMap;

const float PI = 3.14159265359;

//Bright mid-day sun
const vec3 lightColor = vec3(1.0, 0.95, 0.9);

//Positions in tangent view space
in VS_OUT {
	vec3 fragPos;
	vec3 lightDir;
	vec2 texCoords;
} fs_in;

//Trowbridge-Reitz GGX for normal distribution function
float distributionGGX(float NdotH, float roughness){
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH2 = NdotH * NdotH;
	float denom = NdotH2 * (a2 - 1.0) + 1.0;
	denom = PI * denom * denom;

	return a2 / denom;
}

//Smith's method with Schlick-GGX for geometric shadowing.  k = (a + 1)^2 / 8
float geometrySmith(float NdotV, float NdotL, float roughness){
	float r = roughness + 1.0;
	float k = r * r * 0.125;
	float invK = 1.0 - k;

	//ggx1 * ggx2
	float numerator = NdotL * NdotV;
	float denom = (NdotL * invK + k) * (NdotV * invK + k);

	return numerator / denom;
}

//Fresnel-Schlick approximation (unrolled x^5)
vec3 fresnelSchlick(float cosTheta, vec3 F0){
	float x = clamp(1.0 - cosTheta, 0.0, 1.0);
	float x2 = x * x;
	float x4 = x2 * x2;
	float x5 = x4 * x;

	return F0 + (1.0 - F0) * x5;
}

//ACES film tone mapping (Krzysztof Narkowicz)
vec3 ACESFilm(vec3 x) {
float a = 2.51f;
float b = 0.03f;
float c = 2.43f;
float d = 0.59f;
float e = 0.14f;

return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
	//material properties, remap normal from [0, 1] to [-1, 1]
	vec3 albedo = texture(albedoMap, fs_in.texCoords).rgb;
	vec3 normal = texture(normalMap, fs_in.texCoords).rgb * 2.0 - 1.0; 
	vec3 occlusionRoughMetalMap = texture(occlusionRoughMetalMap, fs_in.texCoords).rgb;
	float ao = occlusionRoughMetalMap.r;
	float roughness = occlusionRoughMetalMap.g;
	float metallic = occlusionRoughMetalMap.b;

	//vectors
	vec3 N = normalize(normal);
	vec3 V = normalize(-fs_in.fragPos);
	vec3 L = normalize(-fs_in.lightDir);
	vec3 H = normalize(L + V);
	
	//dot products
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float NdotH = max(dot(N, H), 0.0);
	float HdotV = max(dot(H, V), 0.0);

	//Fresnel-Shlick base reflectivity
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	//reflectance equation
	vec3 Lo = vec3(0.0);

	//Cook-Torrance BRDF
	float NDF = distributionGGX(NdotH, roughness);
	float G = geometrySmith(NdotV, NdotL, roughness);
	vec3 F = fresnelSchlick(HdotV, F0);

	//specular and diffuse terms
	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;
	kD *= 1.0 - metallic;

	//Avoid division by 0 using a small amount of the roughness
	vec3 numerator = NDF * G * F;
	float denom = max(4.0 * NdotV * NdotL, 0.01 + 0.01 * roughness); 
	vec3 specular = numerator / denom;

	//add to outgoing radiance Lo
	Lo += (kD * albedo / PI + specular) * lightColor * NdotL;

	vec3 ambient = vec3(0.1) * albedo * ao;
	vec3 color = ambient + Lo;

	//ACES tone mapping
	color = ACESFilm(color);

	fragColor = vec4(color, 1.0);
}
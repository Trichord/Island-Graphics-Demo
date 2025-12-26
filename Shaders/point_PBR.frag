#version 460 core

out vec4 fragColor;

uniform sampler2D albedoMap;
uniform sampler2D occlusionRoughMetalMap;
uniform sampler2D normalMap;
uniform float gamma;

const float INVERSE_PI = 0.31830988618;

const vec3 lightColor = vec3(2.5);

//Positions in tangent view space
in VS_OUT {
	vec3 cameraPos;
	vec3 lightPos;
	vec3 fragPos;
	vec2 texCoords;
} fs_in;

//Trowbridge-Reitz GGX for normal distribution function
float distributionGGX(float NdotH, float roughness){
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH2 = NdotH * NdotH;
	float denom = fma(NdotH2, a2 - 1.0, 1.0);
	float denom2 = denom * denom;

	return a2 * INVERSE_PI / denom2;
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

//Burely diffuse (unrolled)
float burleyDiffuseFactor(float FD90, float NdotL, float NdotV){
	float L = 1.0 - NdotL;
	float L2 = L * L;
	float L4 = L2 * L2;
	float L5 = L4 * L;

	float V = 1.0 - NdotV;
	float V2 = V * V;
	float V4 = V2 * V2;
	float V5 = V4 * V;

	float FDminusOne = FD90 - 1.0;

	return (1.0 + FDminusOne * L5) * (1.0 + FDminusOne * V5); 
}

//ACES film tone mapping (Krzysztof Narkowicz)
vec3 ACESFilm(vec3 x) {
const float a = 2.51;
const float b = 0.03;
const float c = 2.43;
const float d = 0.59;
const float e = 0.14;

return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
	//material properties, remap normal from [0, 1] to [-1, 1]
	vec4 albedo = texture(albedoMap, fs_in.texCoords);
	vec3 normal = texture(normalMap, fs_in.texCoords).rgb * 2.0 - 1.0; 
	vec3 occlusionRoughMetalMap = texture(occlusionRoughMetalMap, fs_in.texCoords).rgb;
	float ao = occlusionRoughMetalMap.r;
	float roughness = occlusionRoughMetalMap.g;
	float metallic = occlusionRoughMetalMap.b;

	vec3 lightDir = fs_in.lightPos - fs_in.fragPos;

	//vectors
	vec3 N = normalize(normal);
	vec3 V = normalize(fs_in.cameraPos - fs_in.fragPos);
	vec3 L = normalize(lightDir);
	vec3 H = normalize(L + V);
	
	//dot products
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float NdotH = max(dot(N, H), 0.0);
	float HdotV = max(dot(H, V), 0.0);

	//Fresnel-Shlick base reflectivity
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo.rgb, metallic);

	//reflectance equation
	vec3 Lo = vec3(0.0);

	//radiance
	float lightDistance = length(lightDir);
	float attenuation = 1.0 / (lightDistance * lightDistance);
	vec3 radiance = lightColor * attenuation;

	//Cook-Torrance BRDF
	float NDF = distributionGGX(NdotH, roughness);
	float G = geometrySmith(NdotV, NdotL, roughness);
	vec3 F = fresnelSchlick(HdotV, F0);

	//specular and diffuse terms
	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;
	kD *= 1.0 - metallic;

	//Avoid division by 0 with a small constant
	vec3 numerator = NDF * G * F;
	float denom = max(4.0 * NdotL * NdotV, 0.0001); 
	vec3 specular = numerator / denom;

	//Burley diffuse
	float FD90 = 0.5 + 2 * roughness * (HdotV * HdotV);
	float burleyFactor = burleyDiffuseFactor(FD90, NdotL, NdotV);
	vec3 diffuse = kD * albedo.rgb * INVERSE_PI * burleyFactor;

	//add to outgoing radiance Lo
	Lo += (diffuse + specular) * radiance * NdotL;

	vec3 ambient = vec3(0.15) * albedo.rgb * ao;
	vec3 color = ambient + Lo;

	//ACES tone mapping
	color = ACESFilm(color);

	//Gamma correction
	color = pow(color, vec3(1.0/gamma));

	fragColor = vec4(color, albedo.a);
}
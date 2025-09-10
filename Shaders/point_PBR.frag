#version 330 core

out vec4 fragColor;

uniform sampler2D albedoMap;
uniform sampler2D metallicRoughnessMap;
uniform sampler2D normalMap;
uniform sampler2D aoMap;

const float PI = 3.14159265359;

vec3 lightColor = vec3(5.0);

in VS_OUT {
	vec3 tangentLightPosition;
	vec3 tangentCameraPos;
	vec3 tangentWorldPos;
	vec2 texCoords;
} fs_in;

//Trowbridge-Reitz GGX for normal distribution function
float distributionGGX(vec3 N, vec3 H, float roughness){
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;
	float denom = NdotH2 * (a2 - 1.0) + 1.0;
	denom = PI * denom * denom;

	return a2 / denom;
}

//Schlick-GGX for geometric shadowing
float geometrySchlickGGX(float NdotV, float roughness){
	float r = roughness + 1.0;
	float k = r * r * 0.125;
	float denom = NdotV * (1.0 - k) + k;

	return NdotV / denom;
}

//Smith's method
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness){
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx1 = geometrySchlickGGX(NdotL, roughness);
	float ggx2 = geometrySchlickGGX(NdotV, roughness);

	return ggx1 * ggx2;
}

//Fresnel-Schlick approximation (unrolled x^5)
vec3 fresnelSchlick(float cosTheta, vec3 F0){
	float x = clamp(1.0 - cosTheta, 0.0, 1.0);
	float x2 = x * x;
	float x4 = x2 * x2;
	float x5 = x4 * x;

	return F0 + (1.0 - F0) * x5;
}

void main() {
	vec3 albedo = texture(albedoMap, fs_in.texCoords).rgb;
	vec3 normal = texture(normalMap, fs_in.texCoords).rgb * 2.0 - 1.0; //From [0, 1] to [-1, 1]
	vec2 metallicRoughness = texture(metallicRoughnessMap, fs_in.texCoords).rg;
	float metallic = metallicRoughness.r;
	float roughness = metallicRoughness.g;
	float ao = texture(aoMap, fs_in.texCoords).r;
	ao = mix(0.4, 1.0, ao);

	vec3 N = normalize(normal);
	vec3 V = normalize(fs_in.tangentCameraPos - fs_in.tangentWorldPos);

	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	//reflectance equation
	vec3 Lo = vec3(0.0);

	//per-light radiance
	float lightDistance = length(fs_in.tangentLightPosition - fs_in.tangentWorldPos);
	float attenuation = 1.0 / (lightDistance * lightDistance);
	vec3 radiance = lightColor * attenuation;

	//Cook-Torrance BRDF
	vec3 L = normalize(fs_in.tangentLightPosition - fs_in.tangentWorldPos);
	vec3 H = normalize(L + V);

	float NDF = distributionGGX(N, H, roughness);
	float G = geometrySmith(N, V, L, roughness);
	vec3 F = fresnelSchlick(clamp(dot(H, V), 0.0, 1.0), F0);

	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;
	kD *= 1.0 - metallic;

	vec3 numerator = NDF * G * F;
	float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.000001;
	vec3 specular = numerator / denominator;

	//add to outgoing radiance Lo
	float NdotL = max(dot(N, L), 0.0);
	Lo += (kD * albedo / PI + specular) * radiance * NdotL;

	vec3 ambient = vec3(0.03) * albedo * ao;

	vec3 color = ambient + Lo;

	//Reinhard tone mapping
	color = color / (color + vec3(1.0));

	fragColor = vec4(color, 1.0);
}
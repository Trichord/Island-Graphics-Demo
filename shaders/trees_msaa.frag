#version 460 core

out vec4 fragColor;

uniform sampler2D albedoMap;
uniform sampler2D occlusionRoughMetalMap;
uniform sampler2D normalMap;
uniform vec3 ambientLight;
uniform float gamma;

const float INVERSE_PI = 0.31830988618;
const float mipScale = 0.25;
const float alphaCutoff = 0.5;

//Bright mid-day sun
const vec3 lightColor = vec3(1.0, 0.95, 0.9);

//Positions in tangent view space
in VS_OUT {
	vec3 cameraPos;
	vec3 lightDir;
	vec3 fragPos;
	vec2 texCoords;
} fs_in;

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

	// Rescale alpha by partial derivative. Credit: Ben Golus "Anti-aliased Alpha Test: The Esoteric Alpha To Coverage"
    albedo.a = (albedo.a - alphaCutoff) / max(fwidth(albedo.a), 0.0001) + 0.5;
  
	//vectors
	vec3 N = normalize(normal);
	vec3 V = normalize(fs_in.cameraPos -fs_in.fragPos);
	vec3 L = normalize(-fs_in.lightDir);
	vec3 H = normalize(L + V);
	
	//dot products
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float HdotV = max(dot(H, V), 0.0);

	//Alter diffuse by metallic-ness of the material
	vec3 kD = vec3(1.0);
	kD *= 1.0 - metallic;

	//Burley diffuse
	float FD90 = 0.5 + 2 * roughness * (HdotV * HdotV);
	float burleyFactor = burleyDiffuseFactor(FD90, NdotL, NdotV);
	vec3 diffuse = kD * albedo.rgb * INVERSE_PI * burleyFactor;

	vec3 accumulatedLight = diffuse * lightColor * NdotL;
	
	//Ambient
	vec3 ambient = ambientLight * albedo.rgb * ao;
	
	//Final color
	vec3 color = ambient + accumulatedLight;

	//ACES tone mapping
	color = ACESFilm(color);

	//Gamma correction
	color = pow(color, vec3(1.0/gamma));

	fragColor = vec4(color, albedo.a);
}



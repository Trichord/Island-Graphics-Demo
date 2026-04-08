#version 460 core

layout (std140, binding = 1) uniform shadowCascadeData
{
	mat4 matrices[3];
	vec4 splits;
};

out vec4 fragColor;

uniform sampler2DArray arrayRGB4k;
uniform sampler2DArray arraySRGB4k;
uniform sampler2DArray arraySRGBA4k;
uniform sampler2DArray arrayRGB2k;
uniform sampler2DArray arraySRGB2k;
uniform sampler2DArray arraySRGBA2k;
uniform sampler2DArray arrayRGB1k;
uniform sampler2DArray arraySRGB1k;
uniform sampler2DArray arraySRGBA1k;
uniform sampler2DArray arrayRGB512;
uniform sampler2DArray arrayRGB256;

uniform sampler2DArray shadowMap;

uniform int albedoArrayTexture;
uniform int albedoLayer;

uniform int armArrayTexture;
uniform int armLayer;

uniform int normalArrayTexture;
uniform int normalLayer;

uniform vec3 ambientLight;
uniform vec3 lightColor;
uniform float gamma;

const float INVERSE_PI = 0.31830988618;
const float minVariance = 0.000001;
const int cascadeCount = 3;
const float lightBleedReduction = 0.2;

//Positions in tangent view space
in VS_OUT 
{
	vec3 cameraPos;
	vec3 lightDir;
	vec3 tangentFragPos;
	vec2 texCoords;
	vec4 viewFragPos;
	vec4 worldFragPos;
} fs_in;

//Trowbridge-Reitz GGX for normal distribution function
float distributionGGX(float NdotH, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH2 = NdotH * NdotH;
	float denom = fma(NdotH2, a2 - 1.0, 1.0);
	float denom2 = denom * denom;

	return a2 * INVERSE_PI / denom2;
}

//Smith's method with Schlick-GGX for geometric shadowing.  k = (a + 1)^2 / 8
float geometrySmith(float NdotV, float NdotL, float roughness)
{
	float r = roughness + 1.0;
	float k = r * r * 0.125;
	float invK = 1.0 - k;

	//ggx1 * ggx2
	float numerator = NdotL * NdotV;
	float denom = (NdotL * invK + k) * (NdotV * invK + k);

	return numerator / denom;
}

//Fresnel-Schlick approximation (unrolled x^5)
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	float x = clamp(1.0 - cosTheta, 0.0, 1.0);
	float x2 = x * x;
	float x4 = x2 * x2;
	float x5 = x4 * x;

	return F0 + (1.0 - F0) * x5;
}

//Burely diffuse (unrolled)
float burleyDiffuseFactor(float FD90, float NdotL, float NdotV)
{
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

//Andrew Lauritzen - "Summed-Area Variance Shadow Maps"
float ChebyshevUpperBound(vec2 moments, float t)
{
  // One-tailed inequality valid if t > Moments.x
	float p = step(t, moments.x);

	// Compute variance.
	float variance = moments.y - (moments.x * moments.x);
	variance = max(variance, minVariance);

	// Compute probabilistic upper bound.
	float d = t - moments.x;
	float pMax = variance / (variance + d * d);

	//Reduce light bleeding
	pMax = smoothstep(lightBleedReduction, 1.0, pMax);
	return max(p, pMax);
}

int calculateCascadeIndex(float depth)
{
	if (splits.x > depth)
		return 0;

	else if (splits.y > depth)
		return 1;

	else
		return 2;
}

float ShadowContribution(vec2 LightTexCoord, float DistanceToLight, int layer)
{	
	// Read the moments from the variance shadow map.
	vec2 moments = texture(shadowMap, vec3(LightTexCoord.xy, float(layer))).rg;

	// Compute the Chebyshev upper bound.
	return ChebyshevUpperBound(moments, DistanceToLight);
}

//ACES film tone mapping (Krzysztof Narkowicz)
vec3 ACESFilm(vec3 x) 
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;

	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec4 textureFromSampler2DArray(vec2 textureCoordinates, int index, float layer)
{
	vec4 tex;
	
	switch (index)
	{
		case 0:
			tex = texture(arrayRGB4k, vec3(textureCoordinates, layer));
			break;
	
		case 1:
			tex = texture(arraySRGB4k, vec3(textureCoordinates, layer));
			break;

		case 2:
			tex = texture(arraySRGBA4k, vec3(textureCoordinates, layer));
			break;

		case 3:
			tex = texture(arrayRGB2k, vec3(textureCoordinates, layer));
			break;

		case 4:
			tex = texture(arraySRGB2k, vec3(textureCoordinates, layer));
			break;

		case 5:
			tex = texture(arraySRGBA2k, vec3(textureCoordinates, layer));
			break;

		case 6:
			tex = texture(arrayRGB1k, vec3(textureCoordinates, layer));
			break;
		
		case 7:
			tex = texture(arraySRGB1k, vec3(textureCoordinates, layer));
			break;

		case 8:
			tex = texture(arraySRGBA1k, vec3(textureCoordinates, layer));
			break;

		case 9:
			tex = texture(arrayRGB512, vec3(textureCoordinates, layer));
			break;

		case 10:
			tex = texture(arrayRGB256, vec3(textureCoordinates, layer));
			break;
		
		default:
			tex = vec4(1.0, 0.0, 1.0, 1.0);
	}

	return tex;
}

void main() {
	//material properties, remap normal from [0, 1] to [-1, 1]
	vec4 albedo = textureFromSampler2DArray(fs_in.texCoords, albedoArrayTexture, albedoLayer);
	vec3 normal = textureFromSampler2DArray(fs_in.texCoords, normalArrayTexture, normalLayer).rgb * 2.0 - 1.0; 
	vec3 armMap = textureFromSampler2DArray(fs_in.texCoords, armArrayTexture, armLayer).rgb;

	//vectors
	vec3 N = normalize(normal);
	vec3 V = normalize(fs_in.cameraPos -fs_in.tangentFragPos);
	vec3 L = normalize(-fs_in.lightDir);
	vec3 H = normalize(L + V);
	
	//dot products
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float NdotH = max(dot(N, H), 0.0);
	float HdotV = max(dot(H, V), 0.0);

	float ao = armMap.r;
	float roughness = armMap.g;
	float metallic = armMap.b;

	//Fresnel-Shlick base reflectivity
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo.rgb, metallic);

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

	//Avoid division by 0, using a small constant
	vec3 numerator = NDF * G * F;
	float denom = max(4.0 * NdotL * NdotV, 0.0001);
	vec3 specular = numerator / denom;

	//Burley diffuse
	float FD90 = 0.5 + 2 * roughness * (HdotV * HdotV);
	float burleyFactor = burleyDiffuseFactor(FD90, NdotL, NdotV);
	vec3 fD = albedo.rgb * INVERSE_PI * burleyFactor;

	vec3 diffuse = kD * fD;

	//add to outgoing radiance Lo
	Lo += (diffuse + specular) * lightColor * NdotL;

	//Pick shadow cascade off frag depth
	int layer = calculateCascadeIndex(-fs_in.viewFragPos.z);

	//Shadows
	vec3 shadowCoords = vec3(matrices[layer] * fs_in.worldFragPos);
	shadowCoords = shadowCoords * 0.5 + 0.5;
	float shadow = ShadowContribution(shadowCoords.xy, shadowCoords.z, layer);

	vec3 ambient = ambientLight * albedo.rgb * ao;
	vec3 color = ambient + Lo * shadow;

	//ACES tone mapping
	color = ACESFilm(color);

	//Gamma correction
	color = pow(color, vec3(1.0/gamma));

	fragColor = vec4(color, albedo.a);
}
#version 460 core

out vec4 fragColor;

uniform sampler2DArray arrayRGB2k;
uniform sampler2DArray arraySRGB2k;
uniform sampler2DArray arraySRGBA2k;

uniform vec3 ambientLight;
uniform vec3 lightColor;
uniform vec3 lightDir;

uniform float gamma;
uniform float mipScale;
uniform float alphaCutoff;

uniform int albedoArrayTexture;
uniform int albedoLayer;

uniform int armArrayTexture;
uniform int armLayer;

const float INVERSE_PI = 0.31830988618;

layout (std140, binding = 0) uniform cameraData {
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
};

in VS_OUT {
	vec3 fragPos;
	vec3 normal;
	vec2 texCoords;
} fs_in;

vec4 textureFromSampler2DArray(vec2 textureCoordinates, int index, float layer)
{
	vec4 tex;
	
	switch (index)
	{
		case 2:
			tex = texture(arrayRGB2k, vec3(textureCoordinates, layer));
			break;

		case 3:
			tex = texture(arraySRGB2k, vec3(textureCoordinates, layer));
			break;

		case 4:
			tex = texture(arraySRGBA2k, vec3(textureCoordinates, layer));
			break;

		default:
			tex = vec4(1.0, 0.0, 1.0, 1.0);
	}

	return tex;
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

//Credit: Ben Golus "Anti-aliased Alpha Test: The Esoteric Alpha To Coverage"
float CalcMipLevel(vec2 texture_coord){
    vec2 dx = dFdx(texture_coord);
    vec2 dy = dFdy(texture_coord);
    float delta_max_sqr = max(dot(dx, dx), dot(dy, dy));
                
    return max(0.0, 0.5 * log2(delta_max_sqr));
}

void main() {
	//material properties, remap normal from [0, 1] to [-1, 1]
	vec4 albedo = textureFromSampler2DArray(fs_in.texCoords, albedoArrayTexture, albedoLayer);
	vec3 normal = normalize(fs_in.normal);
	vec3 armMap = textureFromSampler2DArray(fs_in.texCoords, armArrayTexture, armLayer).rgb;
	float ao = armMap.r;
	float roughness = armMap.g;
	float metallic = armMap.b;

	//Scale alpha by mip level
    vec2 texSize = vec2(2048, 2048);
    albedo.a *= 1 + max(0, CalcMipLevel(fs_in.texCoords * texSize)) * mipScale;

	// Rescale alpha by partial derivative. Credit: Ben Golus
    albedo.a = (albedo.a - alphaCutoff) / max(fwidth(albedo.a), 0.0001) + 0.5;

	//vectors
	vec3 N = normalize(normal);
	vec3 V = normalize(cameraPosition.xyz - fs_in.fragPos);
	vec3 L = normalize(-lightDir);
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



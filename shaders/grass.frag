#version 460 core

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

uniform int albedoArrayTexture;
uniform int albedoLayer;

uniform vec3 ambientLight;
uniform vec3 lightDir;
uniform vec3 lightColor;

uniform float gamma;
uniform float mipScale;
uniform float alphaCutoff;

in vec2 texCoords;
in vec3 normal;

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

vec2 sizeFromBucketIndex(int index)
{
    if (index <= 2) return vec2(4096.0);
    if (index <= 5) return vec2(2048.0);
    if (index <= 8) return vec2(1024.0);
    if (index == 9) return vec2(512.0);
    return vec2(256.0);
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
    vec4 albedo = textureFromSampler2DArray(texCoords, albedoArrayTexture, albedoLayer);

    //Flip normal if 2D quad is facing away
    vec3 normalizedNormal = normalize(normal);
    normalizedNormal = gl_FrontFacing ? normalizedNormal : -normalizedNormal;

    //Light Direction
    vec3 lightDirection = normalize(-lightDir);
    float NdotL = dot(normalizedNormal, lightDirection);

    //Diffuse
    float diffuseStrength = NdotL * 0.5 + 0.6;

	//Scale alpha by mip level 0
    vec2 texSize = sizeFromBucketIndex(albedoArrayTexture);
    albedo.a *= 1 + max(0, CalcMipLevel(texCoords * texSize)) * mipScale;

	// Rescale alpha by partial derivative. Credit: Ben Golus
    albedo.a = (albedo.a - alphaCutoff) / max(fwidth(albedo.a), 0.0001) + 0.5;
    
    //Ambient
    vec3 ambient = ambientLight * lightColor * albedo.rgb;
    
    //Diffuse continued after filling texture load with math
    vec3 diffuse = diffuseStrength * lightColor * albedo.rgb;

    vec3 color = ambient + diffuse;

    //Tone mapping
    color = ACESFilm(color);

    //Gamma correction
	color = pow(color, vec3(1.0/gamma)); 
   
    fragColor = vec4(color, albedo.a);
}
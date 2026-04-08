#version 460 core

out vec4 fragColor;

layout (std140, binding = 0) uniform cameraData {
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
};

//dudv in the arm slot
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

uniform samplerCube cubeMap;

uniform int armArrayTexture;
uniform int armLayer;

uniform int normalArrayTexture;
uniform int normalLayer;

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

const float dudvStrength = 0.01;
const float specularDamper = 0.2;
const vec4 waterColor = vec4(0.01, 0.48, 0.68, 0.0) * 0.2;
const float waterTransparency = 0.35;

float waveSpeed = 0.05;

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

void main() 
{	
	vec3 V = normalize(cameraPosition.xyz - fs_in.fragPos);
	vec3 L = normalize(-lightDir);
	vec3 H = normalize(L + V);

	// Move textures by time
	float movement = time * waveSpeed * 1.0 / (1.0 + log(1.0 + scale.x));

	//Repeat texture based off model scale
	float tileFactor = scale.x / 12.0;

	// Add tiling and time based movement
	vec2 d1 = fs_in.texCoords * tileFactor + vec2(movement, 0.0);
	vec2 d2 = fs_in.texCoords * tileFactor + vec2(0.0, movement);
	
	// Distortion from dudv, remapped to [-1, 1] to sample distortion in all directions
	vec2 distortion1 = (textureFromSampler2DArray(d1, armArrayTexture, armLayer).rg * 2.0 - 1.0) * dudvStrength;	
	vec2 distortion2 = (textureFromSampler2DArray(d2, armArrayTexture, armLayer).xy * 2.0 - 1.0) * dudvStrength;
	
	//Total distortion from dudv
	vec2 totalDistortion = distortion1 + distortion2;

	// Normal, biased towards the sky for less choppy water. Swizzled because the plane is horizontal
	vec2 normalCoords = vec2(fs_in.texCoords * tileFactor + totalDistortion);
	vec3 N = textureFromSampler2DArray(normalCoords, normalArrayTexture, normalLayer).xyz * 2.0 - 1.0;
	N = normalize(vec3(N.x, N.z, -N.y));

	// Specular
	float specular = pow(max(dot(N, H), 0.0), 32.0);
	vec3 specHighlight = lightColor * specular * specularDamper;

	// Fresnel
	float fresnel = 1.0 - dot(N, V);
	fresnel = pow(fresnel, 5.0);

	// Reflection 
	vec3 reflectDir = reflect(-V, N);
	vec3 reflection = texture(cubeMap, reflectDir).rgb;
	
	float alpha = mix(waterTransparency, 1.0, fresnel);

	fragColor = vec4(reflection, alpha) + vec4(specHighlight, 0.0) + waterColor;
}
#version 460 core

const float lightBleedReduction = 0.2f;

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

uniform sampler2DMS shadowMap;

uniform int albedoArrayTexture;
uniform int albedoLayer;

out vec2 fragColor;

in vec2 texCoords;

vec4 textureFromSampler2DArray(int index, float layer)
{
	vec4 tex;
	
	switch (index)
	{
		case 0:
			tex = texture(arrayRGB4k, vec3(texCoords, layer));
			break;
	
		case 1:
			tex = texture(arraySRGB4k, vec3(texCoords, layer));
			break;

		case 2:
			tex = texture(arraySRGBA4k, vec3(texCoords, layer));
			break;

		case 3:
			tex = texture(arrayRGB2k, vec3(texCoords, layer));
			break;

		case 4:
			tex = texture(arraySRGB2k, vec3(texCoords, layer));
			break;

		case 5:
			tex = texture(arraySRGBA2k, vec3(texCoords, layer));
			break;

		case 6:
			tex = texture(arrayRGB1k, vec3(texCoords, layer));
			break;
		
		case 7:
			tex = texture(arraySRGB1k, vec3(texCoords, layer));
			break;

		case 8:
			tex = texture(arraySRGBA1k, vec3(texCoords, layer));
			break;

		case 9:
			tex = texture(arrayRGB512, vec3(texCoords, layer));
			break;

		case 10:
			tex = texture(arrayRGB256, vec3(texCoords, layer));
			break;
		
		default:
			tex = vec4(1.0, 0.0, 1.0, 1.0);
	}

	return tex;
}

//Credit: Andrew Lauritzen
vec2 computeMoments(float depth)
{
  vec2 moments;
  moments.x = depth;
  moments.y = depth * depth;
  return moments;
}

void main()
{	
	float alpha = textureFromSampler2DArray(albedoArrayTexture, albedoLayer).a;

	if (alpha < 0.1)
		discard;

	fragColor = computeMoments(gl_FragCoord.z);
}
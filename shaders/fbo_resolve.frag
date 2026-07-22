#version 460 core

#include "shared/samplers.glsl"

// In
uniform float gamma;

in vec2 texCoords;

// Out
out vec4 fragColor;

// ACES film tone mapping (Krzysztof Narkowicz)
vec3 ACESFilm(vec3 x) 
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;

	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
	vec3 color = texture(resolvedColor, texCoords).rgb;

	color = ACESFilm(color);

	#ifdef GAMMA_CORRECT
	color = pow(color, vec3(1.0/gamma));
	#endif

	fragColor = vec4(color, 1.0);
}
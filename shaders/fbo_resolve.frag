#version 460 core

#include "textures.glsl"
#include "tone_mapping.glsl"

// In
uniform float gamma;

in vec2 texCoords;

// Out
out vec4 fragColor;

void main()
{
	vec3 color = texture(frameBuffer, texCoords).rgb;

	color = ACESFilm(color);

	#ifdef GAMMA_CORRECT
	color = pow(color, vec3(1.0/gamma));
	#endif

	fragColor = vec4(color, 1.0);
}
#version 460 core

const float lightBleedReduction = 0.2f;

layout(binding = 0) uniform sampler2DArray arrayTextures[8];

flat in int albedoArray;
flat in int albedoLayer;

out vec2 fragColor;

in vec2 texCoords;

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
	float alpha = texture(arrayTextures[albedoArray], vec3(texCoords, albedoLayer)).a;

	if (alpha < 0.1)
		discard;

	fragColor = computeMoments(gl_FragCoord.z);
}
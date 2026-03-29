#version 460 core

const float lightBleedReduction = 0.2f;

uniform sampler2DMS shadowMap;
uniform sampler2D albedoMap;

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
	float alpha = texture(albedoMap, texCoords).a;

	if (alpha < 0.1)
		discard;

	fragColor = computeMoments(gl_FragCoord.z);
}
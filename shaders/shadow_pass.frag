#version 460 core

const float lightBleedReduction = 0.2f;

layout(location = 9) uniform sampler2DMS shadowMap;

out vec2 fragColor;

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
	fragColor = computeMoments(gl_FragCoord.z);
}
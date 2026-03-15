#version 460 core

const float lightBleedReduction = 0.2f;

uniform sampler2DMS shadowMap;

out vec2 fragColor;

in VS_OUT {
	centroid float fragDepth; //Depth after light transform, only good to interpolate for directional light
} fs_in;

//Credit: Andrew Lauritzen
vec2 computeMoments(float depth)
{
  vec2 moments;
  moments.x = depth;
  moments.y = depth * depth;
  return moments;
}

/*
for the actual shadow rendering in the PBR shader:
float pMax;
//do stuff

pMax = smoothstep(lightBleedReduction, 1.0, pMax);
*/

void main()
{	
	fragColor = computeMoments(fs_in.fragDepth);
}
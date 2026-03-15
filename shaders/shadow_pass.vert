#version 460 core

layout (location = 0) in vec3 aPos;

uniform mat4 lightViewPerspectiveMat;
uniform mat4 modelMatrix;

out VS_OUT {
	centroid float fragDepth; //Depth after light transform, only good to interpolate for directional light
} vs_out;

void main()
{
	//vs_out.fragDepth = (lightViewPerspectiveMat * modelMatrix * vec4(aPos, 1.0)).z;
}
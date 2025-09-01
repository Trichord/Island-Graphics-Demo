#version 330 core

in vec4 fragPos;

uniform float farPlane;
uniform vec3 lightPos;

void main () {
	float fragDepth = length(fragPos.xyz - lightPos);
	fragDepth = fragDepth / farPlane;
	gl_FragDepth = fragDepth;
}
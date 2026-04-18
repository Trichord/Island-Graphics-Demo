#version 460 core

out vec4 fragColor;

layout(binding = 8) uniform samplerCube cubeMap;

uniform float gamma;

in vec3 texCoords;

void main() {

	vec3 color = texture(cubeMap, texCoords).rgb;
	
	//Gamma correction
	color = pow(color, vec3(1.0/gamma));
	
	fragColor = vec4(color, 1.0); 
}
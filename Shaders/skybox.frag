#version 460 core

in vec3 texCoords;
uniform samplerCube cubeMap;
uniform float gamma;

out vec4 fragColor;

void main() {

	vec3 color = texture(cubeMap, texCoords).rgb;
	
	//Gamma correction
	color = pow(color, vec3(1.0/gamma));
	
	fragColor = vec4(color, 1.0); 
}
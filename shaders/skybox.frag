#version 460 core

out vec4 fragColor;

layout(binding = 8) uniform samplerCubeArray cubemaps[3];

uniform float gamma;

flat in int cubeArray;
flat in int cubeLayer;

in vec3 texCoords;

void main() {

	vec3 color = texture(cubemaps[cubeArray], vec4(texCoords, cubeLayer)).rgb;
	
	//Gamma correction
	color = pow(color, vec3(1.0/gamma));
	
	fragColor = vec4(color, 1.0); 
}
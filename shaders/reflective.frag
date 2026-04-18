#version 460 core

layout (std140) uniform uniformData 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
	layout (offset = 272) float time;
};

in vec3 normal;
in vec3 position;

layout(binding = 8) uniform samplerCube cubeMap;
uniform float gamma;

out vec4 fragColor;

void main() {
	vec3 viewDir = normalize(position - cameraPosition.xyz);
	vec3 reflectDir = reflect(viewDir, normalize(normal));
	vec3 color = texture(cubeMap, reflectDir).rgb;
	color = pow(color, vec3(1.0/gamma));
	
	fragColor = vec4(color, 1.0); 
}
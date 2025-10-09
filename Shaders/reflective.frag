#version 460 core



layout (std140) uniform matrices {
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionViewMatrix;
	layout (offset = 128) vec4 cameraPosition;
};

in vec3 normal;
in vec3 position;

uniform samplerCube cubemap;

out vec4 fragColor;

void main() {
	vec3 viewDir = normalize(position - cameraPosition.xyz);
	vec3 reflectDir = reflect(viewDir, normalize(normal));
	fragColor = vec4(texture(cubemap, reflectDir).rgb, 1.0f);
}
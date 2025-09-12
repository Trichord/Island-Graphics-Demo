#version 330 core

out vec4 fragColor;

in vec3 normal;
in vec3 fragPos;

uniform samplerCube cubemap;

void main() {
	vec3 viewDir = normalize(-fragPos);
	vec3 reflectDir = reflect(viewDir, normalize(normal));
	fragColor = vec4(texture(cubemap, reflectDir).rgb, 1.0f);
}

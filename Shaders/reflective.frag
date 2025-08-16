#version 330 core

out vec4 FragColor;

in vec3 normal;
in vec3 position;

uniform samplerCube skybox;
uniform vec3 cameraPos;

void main() {
	vec3 viewDir = normalize(position - cameraPos);
	vec3 reflectDir = reflect(viewDir, normalize(normal));
	FragColor = vec4(texture(skybox, reflectDir).rgb, 1.0f);
}

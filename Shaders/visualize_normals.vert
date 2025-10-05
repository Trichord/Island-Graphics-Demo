#version 440 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

layout (std140) uniform matrices {
	mat4 viewMatrix;
	mat4 projectionMatrix;
};

uniform mat4 modelMatrix;
uniform mat3 normalMatrix; //In view * model space

out VS_OUT {
	vec3 normalVec;
} vs_out;

void main() {
	gl_Position = viewMatrix * modelMatrix * vec4(aPos, 1.0f);
	vs_out.normalVec = vec3(normalize(normalMatrix * aNormal));
}
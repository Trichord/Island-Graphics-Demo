#version 460 core

layout (location = 0) in vec3 aPos;

uniform mat4 viewProjectionNoTranslation;

out vec3 texCoords;

void main() {
	texCoords = aPos;
	vec4 pos = viewProjectionNoTranslation * vec4(aPos, 1.0f);
	gl_Position = pos.xyww;
}
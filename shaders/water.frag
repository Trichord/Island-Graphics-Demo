#version 460 core

out vec4 fragColor;

layout (std140) uniform matrices 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionViewMatrix;
	layout (offset = 128) vec4 cameraPosition;
};

uniform sampler2D armMap; //dudv map here
uniform sampler2D normalMap;
uniform vec3 ambientLight;
uniform vec3 lightColor;
uniform vec3 lightDir;
uniform float gamma;

const float INVERSE_PI = 0.31830988618;

//Positions in tangent view space
in VS_OUT 
{
	vec3 fragPos;
	vec2 texCoords;
} fs_in;

void main() 
{
	vec4 normalMap = texture(normalMap, fs_in.texCoords);
	vec4 dudvMap = texture(armMap, fs_in.texCoords);
	fragColor = vec4(dudvMap.rgb, 0.5);
}
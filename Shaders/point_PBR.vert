#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aTangent;
layout (location = 3) in vec2 aTexCoords;

layout (std140) uniform matrices {
	mat4 viewMatrix;
	mat4 projectionMatrix;
};

out VS_OUT {
	vec3 tangentLightPosition;
	vec3 tangentCameraPos;
	vec3 tangentWorldPos;
	vec2 texCoords;
} vs_out;

uniform mat4 modelMatrix;
uniform mat3 normalMatrix;
uniform vec3 lightPosition;
uniform vec3 cameraPosition;

void main () {
	//TBN matrix for normal map
	vec3 T = normalize(vec3(modelMatrix * vec4(aTangent.xyz, 0.0)));
	vec3 N = normalize(normalMatrix * aNormal);
	T = normalize(T - dot(T, N) * N); 
	vec3 B = normalize(cross(N, T) * aTangent.w);
	mat3 TBN = transpose(mat3(T, B, N));

	vs_out.texCoords = aTexCoords;
	vs_out.tangentWorldPos = TBN * (modelMatrix * vec4(aPos, 1.0)).xyz;
	vs_out.tangentCameraPos = TBN * cameraPosition;
	vs_out.tangentLightPosition = TBN * lightPosition;
	
	gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.0);
}

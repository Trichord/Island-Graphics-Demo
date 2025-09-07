#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;

layout (std140) uniform matrices {
	mat4 viewMatrix;
	mat4 projectionMatrix;
};

out VS_OUT {
	vec3 tangentLightPositions[4];
	vec3 tangentCameraPos;
	vec3 tangentWorldPos;
	vec2 texCoords;
} vs_out;

uniform mat4 modelMatrix;
uniform mat3 normalMatrix;

uniform vec3 lightPositions[4];
uniform vec3 cameraPos;

void main () {
	//TBN matrix for normal map
	vec3 T = normalize(vec3(modelMatrix * vec4(aTangent, 0.0)));
	vec3 N = normalize(normalMatrix * aNormal);
	T = normalize(T - dot(T, N) * N); 
	vec3 B = cross(N, T);
	mat3 TBN = transpose(mat3(T, B, N));

	vs_out.texCoords = aTexCoords;
	vs_out.tangentWorldPos = TBN * (modelMatrix * vec4(aPos, 1.0)).xyz;
	vs_out.tangentCameraPos = TBN * cameraPos;
	
	for(int i = 0; i < 4; ++i)
		vs_out.tangentLightPositions[i] = TBN * lightPositions[i];
	
	gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(aPos, 1.0);
}

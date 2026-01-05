#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aTangent;
layout (location = 3) in vec2 aTexCoords;
layout (location = 4) in mat4 aTransform;
layout (location = 8) in mat4 aNormalMatrix;

layout (std140) uniform matrices {
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionViewMatrix;
	layout (offset = 128) vec4 cameraPosition;
};

out VS_OUT {
	vec3 cameraPos;
	vec3 lightDir;
	vec3 fragPos;
	vec2 texCoords;
} vs_out;

const float minVertexHeight = 0.05;
const float maxVertexHeight = 17.0;

uniform float time;
uniform float windStrength;
uniform vec3 windDirection;
uniform vec3 lightDir;

vec4 centerPos = aTransform[3];

float random(vec2 st){
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

vec3 calcWindTranslation(float timeStamp, vec3 windDir, float windStr){
	float seedValue = random(aTransform[0].xy) * 0.7;
	
	float time1 = sin(timeStamp + random(centerPos.yx) + seedValue);
	float time2 = cos(timeStamp + random(centerPos.yx * 1.337));

	// Remap waves from [-1, 1] to [-0.05, 0.95] 
	float wave = (time1 * 0.5 + 0.45) * (time2 * 0.5 + 0.45);

	return windDir * wave * windStr;
}

void main() {
	//Shave off padding from mat4
	mat3 normalMatrix = mat3(aNormalMatrix);

	//TBN matrix for normal map.  Transpose maps world -> tangent
	vec3 T = normalize(normalMatrix * aTangent.xyz);
	vec3 N = normalize(normalMatrix * aNormal);
	T = normalize(T - dot(T, N) * N); 
	vec3 B = normalize(cross(N, T) * aTangent.w);
	mat3 TBN = transpose(mat3(T, B, N));

	vs_out.texCoords = aTexCoords;
	vs_out.fragPos = TBN * (aTransform * vec4(aPos, 1.0)).xyz;
	vs_out.lightDir = TBN * lightDir;
	vs_out.cameraPos = TBN * cameraPosition.xyz;

	vec3 windTranslation = calcWindTranslation(time, windDirection, windStrength);
	float mask = smoothstep(minVertexHeight, maxVertexHeight, aPos.y);
	windTranslation *= mask;

	//Exaggerate movement of tree
	windTranslation *= 5.0f;
	
	 gl_Position = projectionViewMatrix * aTransform * vec4(aPos + windTranslation, 1.0);
}
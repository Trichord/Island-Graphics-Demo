#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aTangent;
layout (location = 3) in vec2 aTexCoords;

layout (std140) uniform matrices 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
};

layout(binding = 0, std430) readonly buffer matricesSSBO {
    mat4 instances[];
};

layout(binding = 1, std430) readonly buffer indicesSSBO {
    uint indices[];
};

out VS_OUT {
	vec3 fragPos;
	vec3 normal;
	vec2 texCoords;
} vs_out;

const float minVertexHeight = 0.05;
const float maxVertexHeight = 17.0;
const float swaySpeed = 0.5;
const float PI = 3.14159265358979323846;

uniform uint matricesOffset;
uniform uint indicesOffset;
uniform float time;
uniform float windStrength;
uniform vec3 windDirection;

float random(vec2 st)
{
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

vec3 calcWindTranslation(mat4 transform, float timeStamp, vec3 windDir, float windStr)
{
	float vertexOffset = random(vec2(gl_InstanceID * PI, gl_InstanceID)) * 0.7;
	
	float time1 = sin(timeStamp + random(vec2(gl_InstanceID, gl_InstanceID + vertexOffset)));
	float time2 = cos(timeStamp + random(vec2(gl_InstanceID, gl_InstanceID) * 1.337));

	// Remap waves from [-1, 1] to [-0.05, 0.95] 
	float wave = (time1 * 0.5 + 0.45) * (time2 * 0.5 + 0.45);

	return windDir * wave * windStr;
}

void main() 
{
	uint instanceIndex = indices[indicesOffset + gl_InstanceID];
	mat4 modelMatrix = instances[matricesOffset + instanceIndex];
	mat3 normalMatrix = mat3(modelMatrix);

	vs_out.texCoords = aTexCoords;
	vs_out.fragPos = vec3(modelMatrix * vec4(aPos, 1.0)).xyz;
	vs_out.normal = normalize(normalMatrix * aNormal);

	vec3 windTranslation = calcWindTranslation(modelMatrix, time * swaySpeed, windDirection, windStrength);
	float mask = smoothstep(minVertexHeight, maxVertexHeight, aPos.y);
	windTranslation *= mask;

	//Exaggerate movement of tree
	windTranslation *= 5.0f;
	
	 gl_Position = projectionViewMatrix * modelMatrix * vec4(aPos + windTranslation, 1.0);
}
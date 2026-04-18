#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aTangent;
layout (location = 3) in vec2 aTexCoords;

layout (std140) uniform uniformData 
{
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionMatrix;
	layout (offset = 128) mat4 projectionViewMatrix;
	layout (offset = 192) mat4 inverseViewProjectMatrix;
	layout (offset = 256) vec4 cameraPosition;
	layout (offset = 272) float time;
};

layout(binding = 0, std430) readonly buffer matricesSSBO {
    mat4 instances[];
};

layout(binding = 1, std430) readonly buffer indicesSSBO {
    int indices[];
};

struct Metadata
{
	int albedoArray;
	int albedoLayer;
	int armArray;
	int armLayer;
	int normalArray;
	int normalLayer;
	int matricesBufferBaseIndex;
};

layout(binding = 2, std430) readonly buffer metadataSSBO
{
	Metadata metadata[];
};

uniform int metadataIndex;
uniform int indicesOffset;

uniform float windStrength;
uniform vec3 windDirection;

out vec2 texCoords;
out vec3 normal;

flat out int albedoArray;
flat out int albedoLayer;

const float swaySpeed = 0.5;
const float maxVertexHeight = 1.0;
const float minVertexHeight = 0.001;
const float PI = 3.14159265358979323846;

float random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

vec3 calcWindTranslation(mat4 transform, float timeStamp, vec3 windDir, float windStr){
	float vertexOffset = random(vec2(gl_InstanceID * PI, gl_InstanceID)) * 0.7;
	
	float time1 = sin(timeStamp + random(vec2(gl_InstanceID, gl_InstanceID + vertexOffset)));
	float time2 = cos(timeStamp + random(vec2(gl_InstanceID, gl_InstanceID) * 1.337));

	// Remap waves from [-1, 1] to [-0.05, 0.95] 
	float wave = (time1 * 0.5 + 0.45) * (time2 * 0.5 + 0.45);

	return windDir * wave * windStr;
}

void main() {
	Metadata primitiveData = metadata[metadataIndex];

	int matricesIndex = primitiveData.matricesBufferBaseIndex;
	int instanceIndex = indices[indicesOffset + gl_InstanceID];

	mat4 modelMatrix = instances[matricesIndex + instanceIndex];
	mat3 normalMatrix = mat3(modelMatrix);
	
	texCoords = aTexCoords;
	normal = normalize(normalMatrix * aNormal);

	vec4 worldPos = modelMatrix * vec4(aPos, 1.0);
	vec3 windTranslation = calcWindTranslation(modelMatrix, time * swaySpeed, windDirection, windStrength);
	float mask = smoothstep(minVertexHeight, maxVertexHeight, aPos.y);
	
	windTranslation *= mask;

	albedoArray = primitiveData.albedoArray;
	albedoLayer = primitiveData.albedoLayer;

    gl_Position = projectionViewMatrix * vec4(worldPos.xyz + windTranslation, 1.0);
}
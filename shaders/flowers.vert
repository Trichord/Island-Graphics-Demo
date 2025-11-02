#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec4 aTangent;
layout (location = 3) in vec2 aTexCoords;
layout (location = 4) in mat4 aTransform;
layout (location = 8) in vec3 centerPos;


layout (std140) uniform matrices {
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionViewMatrix;
	layout (offset = 128) vec4 cameraPosition;
};

out vec2 texCoords;

uniform float time;
uniform float windStrength;
uniform vec3 windDirection;

float random(vec2 st)
{
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

vec3 calcWindTranslation(float timeStamp, vec3 windDir, float windStr){
	float vertexOffset = random(aTransform[0].xy) * 0.7;
	
	float time1 = sin(timeStamp + random(centerPos.xz) + vertexOffset);
	float time2 = cos(timeStamp + random(centerPos.xz * 1.337));

	float wave = (time1 * 0.5 + 0.45) * (time2 * 0.5 + 0.45);

	return windDir * wave * windStr;
}

void main() {
	texCoords = aTexCoords; 

	vec4 worldPos = aTransform * vec4(aPos, 1.0);

	vec3 windTranslation = calcWindTranslation(time, windDirection, windStrength);

	float mask = smoothstep(0.01, 1.0, aPos.y);
	windTranslation *= mask;

    gl_Position = projectionViewMatrix * vec4(worldPos.xyz + windTranslation, 1.0);
}
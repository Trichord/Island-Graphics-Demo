#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;
layout (location = 2) in mat4 aTransform;
layout (location = 6) in mat3 aNormalMat;

layout (std140) uniform matrices {
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionViewMatrix;
	layout (offset = 128) vec4 cameraPosition;
};

out vec2 texCoords;
out vec3 normal;

uniform float time;
uniform float windStrength;
uniform vec3 windDirection;

float random(vec2 st)
{
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

vec3 calcWindTranslation(vec3 position, float timeStamp, vec3 windDir, float windStr){
	float time1 = sin(timeStamp + random(gl_InstanceID * position.xz));
	float time2 = cos(timeStamp + random(gl_InstanceID * windStr * position.zy));
	return windDir * (time1 * time2 * windStr);
}

void main() {
	texCoords = aTexCoords; 

	vec3 windTranslation = calcWindTranslation(aTransform[3].xyz, time, windDirection, windStrength);
	float mask = smoothstep(0.0, 0.3, 1.0 - texCoords.y);
	windTranslation *= mask;

	normal = aNormalMat * vec3(0.0, 1.0, 0.0);

    gl_Position = projectionViewMatrix * aTransform * vec4(aPos + windTranslation, 1.0);
}
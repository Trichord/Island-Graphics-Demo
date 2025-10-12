#version 460 core

in VS_OUT {
	vec3 fragPos;
	vec3 normalVector;
	vec2 texCoords;
} fs_in;

uniform sampler2D texture_diffuse1;
uniform samplerCube depthMap;

uniform vec3 viewPos;
uniform vec3 lightPos;
uniform float farPlane;

out vec4 fragColor;

const int SAMPLE_SIZE = 20;
const vec3 sampleOffsetDirections[20] = vec3[] (
   normalize(vec3( 1,  1,  1)), normalize(vec3( 1, -1,  1)), normalize(vec3(-1, -1,  1)), normalize(vec3(-1,  1,  1)), 
   normalize(vec3( 1,  1, -1)), normalize(vec3( 1, -1, -1)), normalize(vec3(-1, -1, -1)), normalize(vec3(-1,  1, -1)),
   normalize(vec3( 1,  1,  0)), normalize(vec3( 1, -1,  0)), normalize(vec3(-1, -1,  0)), normalize(vec3(-1,  1,  0)),
   normalize(vec3( 1,  0,  1)), normalize(vec3(-1,  0,  1)), normalize(vec3( 1,  0, -1)), normalize(vec3(-1,  0, -1)),
   normalize(vec3( 0,  1,  1)), normalize(vec3( 0, -1,  1)), normalize(vec3( 0, -1, -1)), normalize(vec3( 0,  1, -1))
); 

float shadowCalculation (vec3 fragPos, vec3 normal, vec3 lightDir){
	float texelSize = 1.0 / 2048.0; //Depth map resolution
	vec3 fragToLight = fragPos - lightPos;
	float viewDistance = length(fragPos - viewPos);
	float currentDepth = length(fragToLight);
	float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
	float shadow = 0.0;
	float diskRadius = texelSize * (1.0 + (viewDistance / farPlane)) * 10;
	for (int i = 0; i < SAMPLE_SIZE; ++i){
			float closestDepth = texture(depthMap, fragToLight + sampleOffsetDirections[i] * diskRadius).r;
			closestDepth *= farPlane; //From [0,1] back to frustrum-appropriate value
			shadow += currentDepth - bias > closestDepth ? 1.0 : 0.0;
	}
	shadow /= SAMPLE_SIZE;

	//fragColor = vec4(vec3(closestDepth / farPlane), 1.0); //Just for seeing the cubemap shadows

	return shadow;
}

void main() {
	vec3 color = vec3(texture(texture_diffuse1, fs_in.texCoords));
	vec3 normalizedNormal = normalize(fs_in.normalVector);
	vec3 lightColor = vec3(0.75);

	//ambient
	vec3 ambient = 0.3 * lightColor;

	//diffuse
	vec3 lightDir = normalize(lightPos - fs_in.fragPos);
	float diffStrength = max(dot(lightDir, normalizedNormal), 0.0);
	vec3 diffuse = diffStrength * lightColor;

	//specular
	vec3 viewDir = normalize(viewPos - fs_in.fragPos);
	vec3 halfwayVector = normalize(lightDir + viewDir);
	float specStrength = pow(max(dot(normalizedNormal, halfwayVector), 0.0), 64);
	vec3 specular = specStrength * lightColor;

	float shadow = shadowCalculation(fs_in.fragPos, normalizedNormal, lightDir);
	vec3 finalColor = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;

	fragColor = vec4(finalColor, 1.0);
}
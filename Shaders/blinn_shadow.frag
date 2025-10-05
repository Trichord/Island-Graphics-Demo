#version 430 core

in VS_OUT {
	vec3 fragPos;
	vec3 normalVector;
	vec2 texCoords;
	vec4 fragPosLightSpace;
} fs_in;

uniform sampler2D texture_diffuse1;
uniform sampler2D depthMap;

uniform vec3 viewPos;
uniform vec3 lightPos;

out vec4 fragColor;

float shadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir){
	vec3 projectedCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	projectedCoords = projectedCoords * 0.5 + 0.5; // Now in [0, 1]
	float closestDepth = texture(depthMap, projectedCoords.xy).r;
	float currentDepth = projectedCoords.z;
	float shadowBias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
	float shadow = 0.0;
	vec2 texelResolution = 1.0 / textureSize(depthMap, 0);

	for (int x = -2; x <= 2; x++){
		for (int y = -2; y <= 2; y++){
			float offsetDepth = texture(depthMap, projectedCoords.xy + vec2(x, y) * texelResolution).r;
			shadow += currentDepth - shadowBias > offsetDepth ? 1.0 : 0.0;
		}
	}

	shadow /= 25.0;

	if (projectedCoords.z > 1.0)
		shadow = 0.0;

	return shadow;
}

void main() {
	vec3 color = vec3(texture(texture_diffuse1, fs_in.texCoords));
	vec3 normalizedNormal = normalize(fs_in.normalVector);
	vec3 lightColor = vec3(1.0);

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

	float shadow = shadowCalculation(fs_in.fragPosLightSpace, normalizedNormal, lightDir);
	vec3 finalColor = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;

	fragColor = vec4(finalColor, 1.0);
}
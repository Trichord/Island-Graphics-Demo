#version 460 core

out vec4 fragColor;

layout (std140) uniform matrices {
	layout (offset = 0) mat4 viewMatrix;
	layout (offset = 64) mat4 projectionViewMatrix;
	layout (offset = 128) vec4 cameraPosition;
};

uniform sampler2D albedoMap;
uniform vec3 ambientLight;
uniform vec3 lightColor;
uniform vec3 lightDir;
uniform float gamma;

const float INVERSE_PI = 0.31830988618;
//Blinn Phong without specular is darker than PBR, trying to roughly match the two shaders
const float blinnBoost = 2.0;

in VS_OUT {
	vec3 fragPos;
	vec3 normal;
	vec2 texCoords;
} fs_in;

//ACES film tone mapping (Krzysztof Narkowicz)
vec3 ACESFilm(vec3 x) 
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;

	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
	vec4 color = vec4(texture(albedoMap, fs_in.texCoords));

	if (color.a < 0.1)
		discard;

	vec3 N = normalize(fs_in.normal);

	//ambient
	vec3 ambient = ambientLight * color.rgb * blinnBoost;

	//diffuse
	vec3 L = normalize(-lightDir);
	float diffStrength = max(dot(L, N), 0.0);
	vec3 diffuse = diffStrength * color.rgb * lightColor * INVERSE_PI;

	vec3 finalColor = ambient + diffuse;

	//ACES tone mapping
	finalColor = ACESFilm(finalColor);

	//Gamma correction
	finalColor = pow(finalColor, vec3(1.0/gamma));

	fragColor = vec4(finalColor, color.a);
}
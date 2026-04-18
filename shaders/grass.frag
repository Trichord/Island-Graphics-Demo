#version 460 core

out vec4 fragColor;

layout (std140, binding = 1) uniform shadowCascadeData
{
	mat4 matrices[3];
	vec4 splits;
};

layout(binding = 0) uniform sampler2DArray arrayTextures[8];

flat in int albedoArray;
flat in int albedoLayer;

uniform vec3 ambientLight;
uniform vec3 lightDir;
uniform vec3 lightColor;

uniform float gamma;
uniform float mipScale;
uniform float alphaCutoff;

in vec2 texCoords;
in vec3 normal;

const float minVariance = 0.000001;
const int cascadeCount = 3;
const float lightBleedReduction = 0.2;

//ACES film tone mapping (Krzysztof Narkowicz)
vec3 ACESFilm(vec3 x) {
const float a = 2.51;
const float b = 0.03;
const float c = 2.43;
const float d = 0.59;
const float e = 0.14;

return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

//Credit: Ben Golus "Anti-aliased Alpha Test: The Esoteric Alpha To Coverage"
float CalcMipLevel(vec2 texture_coord){
    vec2 dx = dFdx(texture_coord);
    vec2 dy = dFdy(texture_coord);
    float delta_max_sqr = max(dot(dx, dx), dot(dy, dy));
                
    return max(0.0, 0.5 * log2(delta_max_sqr));
}

void main() {
    vec4 albedo = texture(arrayTextures[albedoArray], vec3(texCoords, albedoLayer));

    //Flip normal if 2D quad is facing away
    vec3 normalizedNormal = normalize(normal);
    normalizedNormal = gl_FrontFacing ? normalizedNormal : -normalizedNormal;

    //Light Direction
    vec3 lightDirection = normalize(-lightDir);
    float NdotL = dot(normalizedNormal, lightDirection);

    //Diffuse
    float diffuseStrength = NdotL * 0.5 + 0.6;

	//Scale alpha by mip level 0
    vec2 texSize = vec2(textureSize(arrayTextures[albedoArray], 0));
    albedo.a *= 1 + max(0, CalcMipLevel(texCoords * texSize)) * mipScale;

	// Rescale alpha by partial derivative. Credit: Ben Golus
    albedo.a = (albedo.a - alphaCutoff) / max(fwidth(albedo.a), 0.0001) + 0.5;
    
    //Ambient
    vec3 ambient = ambientLight * lightColor * albedo.rgb;
    
    //Diffuse continued after filling texture load with math
    vec3 diffuse = diffuseStrength * lightColor * albedo.rgb;
    
    vec3 color = ambient + diffuse;

    //Tone mapping
    color = ACESFilm(color);

    //Gamma correction
	color = pow(color, vec3(1.0/gamma)); 
   
    fragColor = vec4(color, albedo.a);
}
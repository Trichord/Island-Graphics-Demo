#version 460 core

const vec3 lightColor = vec3(1.0, 0.95, 0.9);
const float alphaCutoff = 0.5;
const float mipScale = 0.25;

out vec4 fragColor;

uniform sampler2D albedoMap;
uniform vec3 ambientLight;
uniform vec3 lightDir;
uniform float gamma;

in vec2 texCoords;
in vec3 normal;

//ACES film tone mapping (Krzysztof Narkowicz)
vec3 ACESFilm(vec3 x) {
const float a = 2.51;
const float b = 0.03;
const float c = 2.43;
const float d = 0.59;
const float e = 0.14;

return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec4 albedo = texture(albedoMap, texCoords);

    //Flip normal if 2D quad is facing away
    vec3 normalizedNormal = normalize(normal);
    normalizedNormal = gl_FrontFacing ? normalizedNormal : -normalizedNormal;

    //Light Direction
    vec3 lightDirection = normalize(-lightDir);
    float NdotL = dot(normalizedNormal, lightDirection);

    //Diffuse
    float diffuseStrength = NdotL * 0.5 + 0.5;

    // Rescale alpha by partial derivative. Credit: Ben Golus "Anti-aliased Alpha Test: The Esoteric Alpha To Coverage"
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
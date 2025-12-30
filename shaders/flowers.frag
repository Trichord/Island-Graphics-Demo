#version 460 core

const vec3 lightColor = vec3(1.0, 0.95, 0.9);
const float alphaCutoff = 0.5;
const float mipScale = 0.25;
const float ambientStrength = 0.2;

out vec4 fragColor;

uniform sampler2D colorTexture;
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
    vec4 grassColor = texture(colorTexture, texCoords);

    //Flip normal if 2D quad is facing away
    vec3 normalizedNormal = normalize(normal);
    normalizedNormal = gl_FrontFacing ? normalizedNormal : -normalizedNormal;

    //Light Direction
    vec3 lightDirection = normalize(-lightDir);
    float NdotL = dot(normalizedNormal, lightDirection);

    // Rescale alpha by partial derivative. Credit: Ben Golus "Anti-aliased Alpha Test: The Esoteric Alpha To Coverage"
    grassColor.a = (grassColor.a - alphaCutoff) / max(fwidth(grassColor.a), 0.0001) + 0.5;
    
    //Ambient
    vec3 ambient = ambientStrength * lightColor * grassColor.rgb;

    //Diffuse
    float diffuseStrength = NdotL * 0.5 + 0.5;
    vec3 diffuse = diffuseStrength * lightColor * grassColor.rgb;

    vec3 color = ambient + diffuse;

    //Tone mapping
    color = ACESFilm(color);

    //Gamma correction
	color = pow(color, vec3(1.0/gamma)); 
   
    fragColor = vec4(color, grassColor.a);
}
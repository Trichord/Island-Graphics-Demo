#version 460 core

#define AMBIENT_STRENGTH 0.8

const vec3 lightColor = vec3(1.0, 0.95, 0.9);

out vec4 fragColor;

uniform sampler2D grassTexture;
uniform vec3 lightDir;
uniform float gamma;

in vec3 normal;
in vec2 texCoords;

void main() {
    vec4 grassColor = texture(grassTexture, texCoords);

    if (grassColor.a < 0.1)
        discard;

    //Flip normal if 2D quad is facing away
    vec3 normalizedNormal = gl_FrontFacing ? normal : -normal;
    normalizedNormal = normalize(normalizedNormal);
    
    //Ambient
    vec3 ambient = AMBIENT_STRENGTH * lightColor;

    //Diffuse
    vec3 lightDirection = normalize(-lightDir);
    float diffuseStrength = max(dot(normalizedNormal, lightDirection), 0.0f);
    vec3 diffuse = diffuseStrength * lightColor;

    //Final color before gamma correction
    vec3 color = (ambient + diffuse) * grassColor.rgb;

    //Gamma correction
	color = pow(color, vec3(1.0/gamma)); 
   
    fragColor = vec4(color, grassColor.a);
}
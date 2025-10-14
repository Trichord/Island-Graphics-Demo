#version 460 core

in vec2 texCoords;

uniform sampler2D grassTexture;
uniform float gamma;

out vec4 fragColor;

void main() {
    vec4 grassColor = texture(grassTexture, texCoords);

    if (grassColor.a < 0.1)
        discard;

    //Gamma correction
	vec3 color = pow(grassColor.rgb, vec3(1.0/gamma)); 
   
   fragColor = vec4(color, grassColor.a);
}
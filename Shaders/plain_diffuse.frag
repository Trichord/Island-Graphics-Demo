#version 440 core

in vec2 texCoords;

uniform sampler2D texture_diffuse1;

out vec4 fragColor;

void main() {    
    fragColor = texture(texture_diffuse1, texCoords);
}
#version 460 core

#include "shared/textures.glsl"

// In
in vec2 texCoords;
flat in int array;
flat in int layer;

// Out
out vec4 fragColor;

void main() 
{
    vec4 albedo = texture(textures[array], vec3(texCoords, layer));
    fragColor = albedo;
}
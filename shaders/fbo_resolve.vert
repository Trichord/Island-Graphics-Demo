#version 460 core

// Constants
const vec3 triangle[3] = vec3[](vec3(-3.0, -1.0, 1.0),
                                vec3( 3.0, -1.0, 1.0),
                                vec3( 0.0,  2.0, 1.0));

// Out
out vec2 texCoords;

void main()
{
    vec3 pos = triangle[gl_VertexID];
    texCoords = pos.xy * 0.5 + 0.5;
    gl_Position = vec4(pos.xy, 0.0, 1.0);
}
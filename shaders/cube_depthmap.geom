#version 460 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;

uniform mat4 shadowMatrices[6];

out vec4 fragPos;

//For each triangle coming in, create new triangles in the 6 light space directions
void main () {
	for (int face = 0; face < 6; ++face){
		gl_Layer = face;
		for (int i = 0; i < 3; ++i){
			fragPos = gl_in[i].gl_Position;
			gl_Position = shadowMatrices[face] * fragPos;
			EmitVertex();
		}
		EndPrimitive();
	}
}
#version 330 core

#define MAGNITUDE 0.4f

layout (triangles) in;
layout (line_strip, max_vertices = 6) out; 

layout (std140) uniform matrices {
	mat4 viewMatrix;
	mat4 projectionMatrix;
};

in VS_OUT {
	vec3 normalVec;
} geo_in[];

void generateLine(int index){
	gl_Position = projectionMatrix * gl_in[index].gl_Position;
	EmitVertex();
	
	gl_Position = projectionMatrix * (gl_in[index].gl_Position + vec4(geo_in[index].normalVec, 0.0f) * MAGNITUDE);
	EmitVertex();

	EndPrimitive();
}

void main() {
	generateLine(0);
	generateLine(1);
	generateLine(2);
}
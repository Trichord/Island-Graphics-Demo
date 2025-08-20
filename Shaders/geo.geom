#version 330 core
layout (points) in;
layout (triangle_strip, max_vertices = 5) out;

in VS_OUT {
	vec3 color;
} geo_in[];

out vec3 color;

void buildHouse(vec4 position){
    color = geo_in[0].color;
    gl_Position = position + vec4(-0.2, -0.2, 0.0, 0.0);    // 1:bottom-left
    EmitVertex();   
    gl_Position = position + vec4( 0.2, -0.2, 0.0, 0.0);    // 2:bottom-right
    EmitVertex();
    gl_Position = position + vec4(-0.2,  0.2, 0.0, 0.0);    // 3:top-left
    EmitVertex();
    gl_Position = position + vec4( 0.2,  0.2, 0.0, 0.0);    // 4:top-right
    EmitVertex();
    gl_Position = position + vec4( 0.0,  0.4, 0.0, 0.0);    // 5:top
    color = vec3(1.0f, 1.0f, 1.0f);
    EmitVertex();
    EndPrimitive();
}

void main() {    
    buildHouse(gl_in[0].gl_Position); 
}  
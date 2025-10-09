#version 460 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

layout (std140) uniform matrices {
	mat4 viewMatrix;
	mat4 projectionMatrix;
};

in VS_OUT {
	vec2 texCoords;
} geo_in[];

out vec2 texCoords;

uniform float time;

vec3 getNormal(){
    vec3 a = vec3(gl_in[0].gl_Position) - vec3(gl_in[1].gl_Position);
    vec3 b = vec3(gl_in[2].gl_Position) - vec3(gl_in[1].gl_Position);
    return normalize(cross(a, b));
}

vec4 explode(vec4 position, vec3 normal){
    float magnitude = 1.5f;
    vec3 direction = normal * ((sin(time) + 1) / 10) * magnitude;
    return position + vec4(direction, 0.0f);
}

void main() {    
    vec3 normal = getNormal();

    mat4 projectionView = projectionMatrix * viewMatrix;
    
    gl_Position = projectionView * explode(gl_in[0].gl_Position, normal);
    texCoords = geo_in[0].texCoords;
    EmitVertex();

    gl_Position = projectionView * explode(gl_in[1].gl_Position, normal);
    texCoords = geo_in[1].texCoords;
    EmitVertex();

    gl_Position = projectionView * explode(gl_in[2].gl_Position, normal);
    texCoords = geo_in[2].texCoords;
    EmitVertex();

    EndPrimitive();
}  
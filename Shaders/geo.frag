#version 430 core

struct MaterialProperties {
    sampler2D texture_diffuse1;
    sampler2D texture_specular1;
};

in vec2 texCoords;
out vec4 fragColor;
uniform MaterialProperties material;

void main() {
    //Compiler will complain if texture_specular1 is not used.  Its a holdover from the old lighting model, to-be-replaced with PBR system
    vec4 tex = texture(material.texture_specular1, texCoords);
	fragColor = texture(material.texture_diffuse1, texCoords) + tex * 0.00001f;
}
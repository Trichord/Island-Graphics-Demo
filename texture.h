#pragma once

#include <glad/glad.h>

enum TextureType {
	TEXTURE_NONE,
	TEXTURE_BASE_COLOR,
	TEXTURE_METALLIC_ROUGHNESS,
	TEXTURE_NORMAL,
	TEXTURE_OCCLUSION
};

struct Texture {
	TextureType type;
	GLuint id;

	Texture(TextureType t = TEXTURE_NONE, GLuint i = 0) : id(i), type(t) {};
};
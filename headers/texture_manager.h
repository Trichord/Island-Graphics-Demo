#pragma once

#include "image_loader.h"
#include <glad/glad.h>

struct Sampler {
	std::uint16_t magFilter, minFilter, wrapS, wrapT;

	Sampler(uint16_t mag = GL_LINEAR, uint16_t min = GL_LINEAR_MIPMAP_LINEAR, uint16_t ws = GL_REPEAT, uint16_t wt = GL_REPEAT) : magFilter(mag), minFilter(min), wrapS(ws), wrapT(wt) {};
};

enum TextureType {
	TEXTURE_NONE,
	TEXTURE_BASE_COLOR,
	TEXTURE_METALLIC_ROUGHNESS,
	TEXTURE_NORMAL,
	TEXTURE_OCCLUSION
};

namespace texture_manager {

GLuint generateTextureID(const ImageData& image, Sampler sampler, unsigned int index, bool sRGB);
}
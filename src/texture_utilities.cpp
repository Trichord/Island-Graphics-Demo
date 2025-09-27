#include "../headers/texture_utilities.h"
#include <glad/glad.h>
#include <iostream>

GLuint texture_utilities::generateTextureID(const ImageData& image, Sampler sampler, unsigned int index, bool sRGB, bool anisotropic) {
	GLuint textureID;

	if (image.data) {
		GLenum imageFormat = 3, internalFormat = 3;
		int nrChannels = image.nrChannels;

		//stb_image only ever returns a 1, 2, 3, or 4 for a valid image
		if (nrChannels == 1) {
			imageFormat = GL_RED;
			internalFormat = GL_RED;
		}
		else if (nrChannels == 2) {
			imageFormat = GL_RG;
			internalFormat = GL_RG;
		}
		else if (nrChannels == 3) {
			imageFormat = GL_RGB;
			internalFormat = sRGB ? GL_SRGB : GL_RGB;
		}
		else if (nrChannels == 4) {
			imageFormat = GL_RGBA;
			internalFormat = sRGB ? GL_SRGB_ALPHA : GL_RGBA;
		}

		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);

		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, image.width, image.height, 0, imageFormat, GL_UNSIGNED_BYTE, image.data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampler.wrapS);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampler.wrapT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampler.minFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampler.magFilter);
	}

	else
		textureID = 0;

	glActiveTexture(GL_TEXTURE0);

	return textureID;
}

GLuint texture_utilities::generateCubemapTextureID(const std::vector<ImageData>& imageData, bool sRGB, bool anisotropic) {
	GLuint cubemapTextureID;
	glGenTextures(1, &cubemapTextureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTextureID);

	// Cubemap ID
	for (unsigned int i = 0; i < 6; ++i) {
		int nrChannels = imageData[i].nrChannels;

		if (imageData[i].data) {
			GLenum imageFormat = 3, internalFormat = 3;

			// stb_image only ever returns a 1, 2, 3, or 4 for a valid image
			if (nrChannels == 1) {
				imageFormat = GL_RED;
				internalFormat = GL_RED;
			}
			else if (nrChannels == 2) {
				imageFormat = GL_RG;
				internalFormat = GL_RG;
			}
			else if (nrChannels == 3) {
				imageFormat = GL_RGB;
				internalFormat = sRGB ? GL_SRGB : GL_RGB;
			}
			else if (nrChannels == 4) {
				imageFormat = GL_RGBA;
				internalFormat = sRGB ? GL_SRGB_ALPHA : GL_RGBA;
			}

			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat, imageData[i].width, imageData[i].height, 0,
				imageFormat, GL_UNSIGNED_BYTE, imageData[i].data);
		}
		else {
			cubemapTextureID = 0;
		}
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glActiveTexture(GL_TEXTURE0);

	return cubemapTextureID;
}
#include "../headers/texture_manager.h"

GLuint texture_manager::generateTextureID(const ImageData& image, Sampler sampler, unsigned int index, bool sRGB) {
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
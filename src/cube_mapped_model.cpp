#include "../headers/cube_mapped_model.h"
#include "../headers/camera.h"
#include "../headers/image_loader.h"
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>

CubeMappedModel::CubeMappedModel(CubeMappedModel&& other) noexcept
	: cubemapTextureID(other.cubemapTextureID) {
	other.cubemapTextureID = 0;
}

CubeMappedModel& CubeMappedModel::operator=(CubeMappedModel&& other) noexcept {
	if (this != &other) {
		cubemapTextureID = other.cubemapTextureID;
		other.cubemapTextureID = 0;
	}

	return *this;
}

CubeMappedModel::~CubeMappedModel() { glDeleteTextures(1, &cubemapTextureID); }

void CubeMappedModel::loadTextures() {
	ImageLoader imageLoader(texturePaths);
	std::vector<ImageData> loadedImages = imageLoader.getImageData();

	glGenTextures(1, &cubemapTextureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTextureID);

	// Cubemap ID
	for (unsigned int i = 0; i < 6; ++i){
		int nrChannels = loadedImages[i].nrChannels;

		if (loadedImages[i].data) {
			GLenum imageFormat = 3;

			// stb_image only ever returns a 1, 2, 3, or 4 for a valid image
			if (nrChannels == 1)
				imageFormat = GL_RED;
			else if (nrChannels == 2)
				imageFormat = GL_RG;
			else if (nrChannels == 3)
				imageFormat = GL_RGB;
			else if (nrChannels == 4)
				imageFormat = GL_RGBA;

			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_SRGB8, loadedImages[i].width, loadedImages[i].height, 0,
				imageFormat, GL_UNSIGNED_BYTE, loadedImages[i].data);
		}
		else {
			cubemapTextureID = 0;
			return;
		}
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glActiveTexture(GL_TEXTURE0);
}

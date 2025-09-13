#pragma once

#include "image_loader.h"
#include <glad/glad.h>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <string>
#include "../headers/camera.h"
#include "shader.h"
#include <glm/glm.hpp>

//Cube mapped models handle their VAO's and VBO's differently
class CubeMappedModel {

public:
	CubeMappedModel() = default;
	CubeMappedModel(const CubeMappedModel&) = delete;
	CubeMappedModel& operator=(CubeMappedModel const&) = delete;
	virtual void draw() = 0;

	CubeMappedModel(CubeMappedModel&& other) noexcept : modelTexID(other.modelTexID) {
		other.modelTexID = 0;
	}

	CubeMappedModel& operator=(CubeMappedModel&& other) noexcept {

		if (this != &other) {
			modelTexID = other.modelTexID;
			other.modelTexID = 0;
		}

		return *this;
	}

	~CubeMappedModel() {
		glDeleteTextures(1, &modelTexID);
	}

protected:
	GLuint modelTexID = 0;
	std::vector<std::filesystem::path> texturePaths;

	void loadTextures() {
		ImageLoader imageLoader(texturePaths);
		std::unordered_map<std::filesystem::path, ImageData> loadedImages = imageLoader.getImageData();

		glGenTextures(1, &modelTexID);
		glBindTexture(GL_TEXTURE_CUBE_MAP, modelTexID);

		//Cubemap ID
		for (auto& [path, image] : loadedImages) {
			int nrChannels = image.nrChannels;

			if (image.data) {
				GLenum imageFormat;

				//stb_image only ever returns a 1, 2, 3, or 4 for a valid image
				if (nrChannels == 1)
					imageFormat = GL_RED;
				else if (nrChannels == 2)
					imageFormat = GL_RG;
				else if (nrChannels == 3)
					imageFormat = GL_RGB;
				else if (nrChannels == 4)
					imageFormat = GL_RGBA;

				std::string face = path.stem().string();

				int cubeTextureTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X;

				if (face == "left")
					cubeTextureTarget = GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
				else if (face == "top")
					cubeTextureTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
				else if (face == "bottom")
					cubeTextureTarget = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
				else if (face == "front")
					cubeTextureTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
				else if (face == "back")
					cubeTextureTarget = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;

				glTexImage2D(cubeTextureTarget, 0, GL_SRGB8, image.width, image.height, 0, imageFormat, GL_UNSIGNED_BYTE, image.data);
			}
			else {
				std::cout << "Cube texture failed to load: " << path.string() << std::endl;
				modelTexID = 0;
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
};
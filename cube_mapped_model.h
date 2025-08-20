#pragma once

#include "image_loader.h"
#include <glad/glad.h>
#include <vector>
#include <unordered_map>
#include <string>
#include "camera.h"
#include "shader.h"
#include <glm/glm.hpp>

//CubeMappedModel owns its texure ID's, VAO, VBO, and EBO.  Therefore copying is disabled
class CubeMappedModel {
public:
	CubeMappedModel() = default;
	
	CubeMappedModel(const CubeMappedModel&) = delete;
	CubeMappedModel& operator=(CubeMappedModel const&) = delete;

	~CubeMappedModel() {
		glDeleteTextures(1, &modelTexID);
		glDeleteBuffers(1, &modelVBO);
		glDeleteVertexArrays(1, &modelVAO);
	}

	virtual void draw() = 0;

protected:
	unsigned int modelVAO = 0, modelVBO = 0, modelEBO = 0, modelTexID = 0;
	std::vector<std::string> texturePaths;

	virtual void setupBuffers() = 0;

	void loadTextures() {
		ImageLoader imageLoader(texturePaths);
		std::unordered_map<std::string, ImageData> loadedImages = imageLoader.getImageData();

		glGenTextures(1, &modelTexID);
		glBindTexture(GL_TEXTURE_CUBE_MAP, modelTexID);

		//Cubemap ID
		for (auto& imagePair : loadedImages) {
			int nrChannels = imagePair.second.nrChannels;
			unsigned char* data = imagePair.second.data;

			if (data) {
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

				std::string path = imagePair.first;
				std::string face = path.substr(path.find_last_of('/') + 1);
				face = face.substr(0, face.find_last_of('.'));

				int cubeTextureTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X;

				if (face == "left")
					cubeTextureTarget = GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
				else if (face == "top")
					cubeTextureTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
				else if (face == "bottom")
					cubeTextureTarget = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
				else if (face == "front") //Flipped, coordinate system mismatch between cubemap and camera
					cubeTextureTarget = GL_TEXTURE_CUBE_MAP_POSITIVE_Z; //OpenGL Spec says this is back, not front
				else if (face == "back")
					cubeTextureTarget = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;

				glTexImage2D(cubeTextureTarget,
					0, GL_RGB, imagePair.second.width, imagePair.second.height, 0, GL_RGB, GL_UNSIGNED_BYTE, data
				);
			}
			else {
				std::cout << "Cube texture failed to load: " << imagePair.first << std::endl;
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
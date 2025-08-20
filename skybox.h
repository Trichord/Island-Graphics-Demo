#pragma once

#include "cube_mapped_model.h"

class Skybox : public CubeMappedModel {
public:
	Skybox() {
		texturePaths = {
			"./Textures/skybox/right.jpg",
			"./Textures/skybox/left.jpg",
			"./Textures/skybox/top.jpg",
			"./Textures/skybox/bottom.jpg",
			"./Textures/skybox/front.jpg",
			"./Textures/skybox/back.jpg"
		};

		loadTextures();
		setupBuffer();
	}

	void draw() {
		glBindVertexArray(modelVAO);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, modelTexID);
		glDepthFunc(GL_LEQUAL);
		glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
		glDepthFunc(GL_LESS);

		glBindVertexArray(0);
	}
protected:
	const float vertices[24] = {
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f
	};

	const unsigned int indices[36] = {
		0, 1, 2,
		2, 3, 0,
		5, 4, 7,
		7, 6, 5,
		4, 0, 3,
		3, 7, 4,
		1, 5, 6,
		6, 2, 1,
		4, 5, 1,
		1, 0, 4,
		3, 2, 6,
		6, 7, 3
	};

	void setupBuffer() override {
		glGenVertexArrays(1, &modelVAO);
		glBindVertexArray(modelVAO);
		glGenBuffers(1, &modelVBO);
		glBindBuffer(GL_ARRAY_BUFFER, modelVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		glGenBuffers(1, &modelEBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, modelEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
		
		glBindVertexArray(0);
	}
};
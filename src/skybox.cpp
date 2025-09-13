#include "../headers/skybox.h"
#include <vector>
#include "glad/glad.h"

Skybox::Skybox(const std::vector<std::string>& skyboxPaths) {
	for (const std::string& imagePath : skyboxPaths)
		texturePaths.emplace_back(imagePath);

	loadTextures();
	setupBuffers();
}

Skybox::~Skybox() {
	glDeleteBuffers(1, &modelVBO);
	glDeleteBuffers(1, &modelEBO);
	glDeleteVertexArrays(1, &modelVAO);
}

void Skybox::draw() {
	glBindVertexArray(modelVAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTextureID);
	glDepthFunc(GL_LEQUAL);
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
	glDepthFunc(GL_LESS);

	glBindVertexArray(0);
}

void Skybox::setupBuffers() {
	glGenVertexArrays(1, &modelVAO);
	glBindVertexArray(modelVAO);
	glGenBuffers(1, &modelVBO);
	glBindBuffer(GL_ARRAY_BUFFER, modelVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glGenBuffers(1, &modelEBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, modelEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
		GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	glBindVertexArray(0);
}
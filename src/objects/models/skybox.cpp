#include "skybox.h"

#include <vector>

#include "cube_mapped_model.h"
#include "glad/glad.h"

Skybox::Skybox(const std::vector<std::filesystem::path>& imagePaths)
    : CubeMappedModel{imagePaths} {
  setupBuffers();
}

Skybox::Skybox(Skybox&& other) noexcept : CubeMappedModel{std::move(other)} {
  modelVAO = other.modelVAO;
  modelVBO = other.modelVBO;
  modelEBO = other.modelEBO;

  other.modelVAO = 0;
  other.modelVBO = 0;
  other.modelEBO = 0;
}

Skybox& Skybox::operator=(Skybox&& other) noexcept {
  if (this != &other) {
    CubeMappedModel::operator=(std::move(other));
    deleteBuffers();

    modelVAO = other.modelVAO;
    modelVBO = other.modelVBO;
    modelEBO = other.modelEBO;

    other.modelVAO = 0;
    other.modelVBO = 0;
    other.modelEBO = 0;
  }

  return *this;
}

Skybox::~Skybox() { deleteBuffers(); }

void Skybox::render() {
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

void Skybox::deleteBuffers() {
  glDeleteBuffers(1, &modelVBO);
  glDeleteBuffers(1, &modelEBO);
  glDeleteVertexArrays(1, &modelVAO);
}
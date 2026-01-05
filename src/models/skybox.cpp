#include "skybox.h"

#include <vector>

#include "cube_mapped_model.h"
#include "glad/glad.h"
#include "texture_manager.h"

Skybox::Skybox(TextureManager& textureManager,
               const std::vector<std::filesystem::path>& imagePaths)
    : CubeMappedModel{textureManager} {
  setupBuffers();
  textureManager.queueCubemapTextureSet(handle, imagePaths, "./");
}

Skybox::Skybox(TextureManager& textureManager, const Skybox& borrowTarget,
               const std::vector<std::filesystem::path>& imagePaths)
    : CubeMappedModel{textureManager, borrowTarget} {
  setupBuffers();
  textureManager.queueCubemapTextureSet(handle, imagePaths, "./");
}

Skybox::Skybox(Skybox&& other) noexcept : CubeMappedModel{std::move(other)} {
  VAO = other.VAO;
  VBO = other.VBO;
  EBO = other.EBO;

  other.VAO = 0;
  other.VBO = 0;
  other.EBO = 0;
}

Skybox::~Skybox() {
  deleteBuffers();
  if (handle) texManager.releaseModel(handle);
}

void Skybox::render() {
  glDepthFunc(GL_LEQUAL);

  if (!textureSet.empty()) glBindTextureUnit(0, textureSet[0].cubemap);

  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

  glDepthFunc(GL_LESS);
}

void Skybox::setupBuffers() {
  glCreateVertexArrays(1, &VAO);

  glEnableVertexArrayAttrib(VAO, 0);

  glCreateBuffers(1, &VBO);
  glNamedBufferStorage(VBO, sizeof(vertices), &vertices[0], 0);

  glCreateBuffers(1, &EBO);
  glNamedBufferStorage(EBO, sizeof(indices), &indices[0], 0);

  glVertexArrayAttribFormat(VAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
  glVertexArrayVertexBuffer(VAO, 0, VBO, 0, 3 * sizeof(float));

  glVertexArrayElementBuffer(VAO, EBO);
  
  glVertexArrayAttribBinding(VAO, 0, 0);
}

void Skybox::deleteBuffers() {
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &EBO);
  glDeleteVertexArrays(1, &VAO);
}
#include "skybox.h"

#include <array>
#include <filesystem>
#include <vector>

#include "gl_state.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "texture_manager.h"

Skybox::Skybox(TextureManager& textureManager,
               const std::vector<std::filesystem::path>& imagePaths)
    : handle{textureManager.assignModelHandle()},
      texManager{textureManager},
      materials{textureManager.acquireUniqueTextureSet(handle)} {
  textureManager.queueCubeMap(handle, imagePaths, "./");
  setupBuffers();
}

Skybox::Skybox(TextureManager& textureManager, const Skybox& borrowTarget,
               const std::vector<std::filesystem::path>& imagePaths)
    : handle{borrowTarget.handle},
      texManager{textureManager},
      materials{textureManager.borrowTextureSet(borrowTarget.handle)} {
  setupBuffers();
}

Skybox::Skybox(Skybox&& other) noexcept
    : materials{other.materials},
      handle{other.handle},
      texManager{other.texManager},
      VAO{other.VAO},
      VBO{other.VBO},
      EBO{other.EBO} {
  other.VAO = 0;
  other.VBO = 0;
  other.EBO = 0;
  other.handle = 0;
}

Skybox::~Skybox() {
  deleteBuffers();
  if (handle) texManager.releaseModel(handle);
}

void Skybox::render(GLState& glState) {
  glDepthFunc(GL_LEQUAL);

  if (!materials.empty()) glBindTextureUnit(3, materials[0].cubemap);

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

bool Skybox::performCulling(const std::array<glm::vec4, 6>& frustumPlanes,
                            const glm::mat4& modelMatrix,
                            const glm::vec3& cameraPos,
                            const float cullDistanceScale) {
  return true;
}
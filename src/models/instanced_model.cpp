#include "instanced_model.h"

#include <fstream>
#include <iostream>
#include <random>

#include "glad/glad.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "texture_manager.h"

InstancedModel::InstancedModel(TextureManager& textureManager,
                               const std::filesystem::path& modelPath,
                               const std::filesystem::path& positionsPath,
                               float minSize, float maxSize,
                               unsigned int numPositions)
    : Model{textureManager, modelPath} {
  positions.reserve(numPositions);
  loadPositions(positionsPath, numPositions);
  generateTransforms(minSize, maxSize);
  setupInstancedBuffers();
}

InstancedModel::InstancedModel(TextureManager& textureManager,
                               const InstancedModel& borrowTarget,
                               const std::filesystem::path& modelPath,
                               const std::filesystem::path& positionsPath,
                               float minSize, float maxSize,
                               unsigned int numPositions)
    : Model{textureManager, borrowTarget, modelPath} {
  loadPositions(positionsPath, numPositions);
  generateTransforms(minSize, maxSize);
  setupInstancedBuffers();
}

InstancedModel::InstancedModel(InstancedModel&& other) noexcept
    : Model{std::move(other)} {
  transformsVBO = other.transformsVBO;
  positionsVBO = other.positionsVBO;
  normalMatricesVBO = other.normalMatricesVBO;
  transforms = std::move(other.transforms);
  normalMatrices = std::move(other.normalMatrices);
  positions = std::move(other.positions);

  other.transformsVBO = 0;
  other.positionsVBO = 0;
  other.normalMatricesVBO = 0;
}

InstancedModel::~InstancedModel() { deleteBuffers(); }

void InstancedModel::drawPrimitive(const Primitive& primitive) {
  glDrawElementsInstanced(GL_TRIANGLES, primitive.getIndicesSize(),
                          GL_UNSIGNED_INT, 0,
                          static_cast<unsigned int>(transforms.size()));
}

void InstancedModel::loadPositions(const std::filesystem::path& positionsPath,
                                   unsigned int numPositions) {
  float x{0.0f};
  float y{0.0f};
  float z{0.0f};

  std::fstream positionsFile{positionsPath};

  if (!positionsFile.is_open()) {
    std::cerr << "Unable to open " << positionsPath << std::endl;
    return;
  }

  while (positionsFile >> x >> y >> z) positions.emplace_back(x, y, z);

  if (positions.size() != numPositions && numPositions != 0)
    std::cout << positionsPath << " expected " << numPositions << " allocated "
              << positions.size() << std::endl;
}

void InstancedModel::generateTransforms(float minSize, float maxSize) {
  std::random_device rd;
  std::mt19937 mt(rd());

  // Rotate the model somewhere in the 360 degree range
  std::uniform_real_distribution<float> randRotation{0.0f, 360.0f};
  std::uniform_real_distribution<float> randScale{minSize, maxSize};

  transforms.reserve(positions.size());
  normalMatrices.reserve(positions.size());

  for (const auto& position : positions) {
    float instanceRotation{randRotation(mt)};
    float instanceScale{randScale(mt)};

    glm::mat4 transform{1.0f};
    transform = glm::translate(transform, position);
    transform = glm::rotate(transform, glm::radians(instanceRotation),
                            glm::vec3{0.0f, 1.0f, 0.0f});
    transform = glm::scale(transform, glm::vec3{instanceScale});

    glm::mat4 normalMatrix{glm::transpose(glm::inverse(glm::mat3{transform}))};

    transforms.emplace_back(std::move(transform));
    normalMatrices.emplace_back(std::move(normalMatrix));
  }
}

/**
@brief Binding points:
1 - transform matrices
2 - normal matrices
*/
void InstancedModel::setupInstancedBuffers() {
  glCreateBuffers(1, &transformsVBO);
  glNamedBufferStorage(transformsVBO, transforms.size() * sizeof(glm::mat4),
                       transforms.data(), GL_DYNAMIC_STORAGE_BIT);

  glCreateBuffers(1, &positionsVBO);
  glNamedBufferStorage(positionsVBO, positions.size() * sizeof(glm::vec3),
                       positions.data(), GL_DYNAMIC_STORAGE_BIT);

  glCreateBuffers(1, &normalMatricesVBO);
  glNamedBufferStorage(normalMatricesVBO,
                       normalMatrices.size() * sizeof(glm::mat4),
                       normalMatrices.data(), GL_DYNAMIC_STORAGE_BIT);

  for (const Primitive::Mesh& mesh : meshes) {
    for (const Primitive& primitive : mesh.primitives) {
      GLuint VAO{primitive.getVAO()};

      // Instanced attributes
      for (GLuint attribIndex : {4, 5, 6, 7, 8, 9, 10, 11})
        glEnableVertexArrayAttrib(VAO, attribIndex);

      glVertexArrayVertexBuffer(VAO, 1, transformsVBO, 0, sizeof(glm::mat4));
      glVertexArrayVertexBuffer(VAO, 2, normalMatricesVBO, 0,
                                sizeof(glm::mat4));

      // Transform matrices layout
      glVertexArrayAttribFormat(VAO, 4, 4, GL_FLOAT, GL_FALSE, 0);
      glVertexArrayAttribFormat(VAO, 5, 4, GL_FLOAT, GL_FALSE,
                                sizeof(glm::vec4));
      glVertexArrayAttribFormat(VAO, 6, 4, GL_FLOAT, GL_FALSE,
                                2 * sizeof(glm::vec4));
      glVertexArrayAttribFormat(VAO, 7, 4, GL_FLOAT, GL_FALSE,
                                3 * sizeof(glm::vec4));

      // Normal matrices layout
      glVertexArrayAttribFormat(VAO, 8, 4, GL_FLOAT, GL_FALSE, 0);
      glVertexArrayAttribFormat(VAO, 9, 4, GL_FLOAT, GL_FALSE,
                                sizeof(glm::vec4));
      glVertexArrayAttribFormat(VAO, 10, 4, GL_FLOAT, GL_FALSE,
                                2 * sizeof(glm::vec4));
      glVertexArrayAttribFormat(VAO, 11, 4, GL_FLOAT, GL_FALSE,
                                3 * sizeof(glm::vec4));

      // Transform matrices
      glVertexArrayAttribBinding(VAO, 4, 1);
      glVertexArrayAttribBinding(VAO, 5, 1);
      glVertexArrayAttribBinding(VAO, 6, 1);
      glVertexArrayAttribBinding(VAO, 7, 1);

      // Normal matrices
      glVertexArrayAttribBinding(VAO, 8, 2);
      glVertexArrayAttribBinding(VAO, 9, 2);
      glVertexArrayAttribBinding(VAO, 10, 2);
      glVertexArrayAttribBinding(VAO, 11, 2);

      // Divisors
      glVertexArrayBindingDivisor(VAO, 1, 1);
      glVertexArrayBindingDivisor(VAO, 2, 1);
    }
  }
}

void InstancedModel::deleteBuffers() {
  glDeleteBuffers(1, &transformsVBO);
  glDeleteBuffers(1, &positionsVBO);
  glDeleteBuffers(1, &normalMatricesVBO);
}
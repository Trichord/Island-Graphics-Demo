#include "instanced_model.h"

#include <glad/glad.h>

#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <random>

InstancedModel::InstancedModel(const std::filesystem::path& modelPath,
                               const std::filesystem::path& positionsPath,
                               unsigned int numPositions)
    : Model{modelPath} {
  loadPositions(positionsPath, numPositions);
  generateTransforms();
  setupInstancedBuffers();
}

InstancedModel::InstancedModel(InstancedModel&& other) noexcept
    : Model{std::move(other)} {
  transformsVBO = other.transformsVBO;
  positionsVBO = other.positionsVBO;
  transforms = std::move(other.transforms);

  other.transformsVBO = 0;
  other.positionsVBO = 0;
}

InstancedModel& InstancedModel::operator=(InstancedModel&& other) noexcept {
  if (this != &other) {
    deleteBuffers();

    Model::operator=(std::move(other));
    transformsVBO = other.transformsVBO;
    positionsVBO = other.positionsVBO;
    transforms = std::move(other.transforms);

    other.transformsVBO = 0;
    other.positionsVBO = 0;
  }
  return *this;
}

InstancedModel::~InstancedModel() { deleteBuffers(); }

void InstancedModel::render() {
  glDisable(GL_CULL_FACE);

  for (const Primitive::Mesh& mesh : meshes) {
    for (const Primitive& primitive : mesh.primitives) {
      int materialIndex{primitive.getMaterialIndex()};
      int albedoIndex{-1};
      int aoMetRoughIdx{-1};
      int normIndex{-1};

      if (materialIndex != -1 && materials.size()) {
        Material material{materials[materialIndex]};

        albedoIndex = material.albedoImageIndex;
        aoMetRoughIdx = material.aoMetallicRoughnessImageIndex;
        normIndex = material.normalImageIndex;
      }

      if (albedoIndex != -1) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, assetImages[albedoIndex]);
      }

      if (aoMetRoughIdx != -1) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, assetImages[aoMetRoughIdx]);
      }

      if (normIndex != -1) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, assetImages[normIndex]);
      }

      glBindVertexArray(primitive.getVAO());
      glDrawElementsInstanced(GL_TRIANGLES, primitive.getIndicesSize(),
                              GL_UNSIGNED_INT, 0,
                              static_cast<unsigned int>(transforms.size()));
    }
  }

  glEnable(GL_CULL_FACE);
  glBindVertexArray(0);
  glActiveTexture(GL_TEXTURE0);
}

void InstancedModel::loadPositions(const std::filesystem::path& positionsPath,
                                   unsigned int numPositions) {
  positions.reserve(numPositions);
  float x{0.0f};
  float y{0.0f};
  float z{0.0f};

  std::fstream positionsFile{positionsPath};

  if (positionsFile.is_open())
    while (positionsFile >> x >> y >> z) positions.emplace_back(x, y, z);

  else
    std::cerr << "Unable to open " << positionsPath << std::endl;
}

void InstancedModel::generateTransforms() {
  std::random_device rd;
  std::mt19937 mt(rd());

  // Rotate the model somewhere in the 360 degree range
  std::uniform_real_distribution<float> baseRotation{0.0f, 360.0f};

  transforms.reserve(positions.size());

  for (const auto& position : positions) {
    float instanceBaseRotation{baseRotation(mt)};

    glm::mat4 transform{1.0f};
    transform = glm::translate(transform, position);
    transform = glm::rotate(transform, glm::radians(instanceBaseRotation),
                            glm::vec3{0.0f, 1.0f, 0.0f});

    transforms.emplace_back(std::move(transform));
  }
}

void InstancedModel::setupInstancedBuffers() {
  // Transforms VBO
  glGenBuffers(1, &transformsVBO);
  glBindBuffer(GL_ARRAY_BUFFER, transformsVBO);
  glBufferData(GL_ARRAY_BUFFER, transforms.size() * sizeof(glm::mat4),
               transforms.data(), GL_STATIC_DRAW);

  // Positions VBO
  glGenBuffers(1, &positionsVBO);
  glBindBuffer(GL_ARRAY_BUFFER, positionsVBO);
  glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3),
               positions.data(), GL_STATIC_DRAW);

  for (const Primitive::Mesh& mesh : meshes) {
    for (const Primitive& primitive : mesh.primitives) {
      glBindVertexArray(primitive.getVAO());

      // Transforms attributes
      glBindBuffer(GL_ARRAY_BUFFER, transformsVBO);
      glEnableVertexAttribArray(4);
      glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                            (void*)0);
      glEnableVertexAttribArray(5);
      glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                            (void*)(sizeof(glm::vec4)));
      glEnableVertexAttribArray(6);
      glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                            (void*)(2 * sizeof(glm::vec4)));
      glEnableVertexAttribArray(7);
      glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                            (void*)(3 * sizeof(glm::vec4)));

      glVertexAttribDivisor(4, 1);
      glVertexAttribDivisor(5, 1);
      glVertexAttribDivisor(6, 1);
      glVertexAttribDivisor(7, 1);

      // Positions attributes
      glBindBuffer(GL_ARRAY_BUFFER, positionsVBO);
      glEnableVertexAttribArray(8);
      glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3),
                            (void*)0);
      glVertexAttribDivisor(8, 1);
    }
  }

  // reset VAO
  glBindVertexArray(0);
}

void InstancedModel::deleteBuffers() {
  glDeleteBuffers(1, &transformsVBO);
  glDeleteBuffers(1, &positionsVBO);
}
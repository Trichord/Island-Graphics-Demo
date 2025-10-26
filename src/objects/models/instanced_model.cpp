#include "instanced_model.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <fstream>

InstancedModel::InstancedModel(const std::filesystem::path& modelPath,
                               const std::filesystem::path& positionsPath,
                               unsigned int numPositions)
    : Model{modelPath} {
  loadPositions(positionsPath, numPositions);
  setupBuffers();
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
void InstancedModel::setupBuffers() {
  // Transforms VBO
  glGenBuffers(1, &transformsVBO);
  glBindBuffer(GL_ARRAY_BUFFER, transformsVBO);
  glBufferData(GL_ARRAY_BUFFER, transforms.size() * sizeof(glm::mat4),
               transforms.data(), GL_STATIC_DRAW);

  // Transforms attributes
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)0);
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

  // Positions VBO
  glGenBuffers(1, &positionsVBO);
  glBindBuffer(GL_ARRAY_BUFFER, positionsVBO);
  glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3),
               positions.data(), GL_STATIC_DRAW);

  // Positions attributes
  glEnableVertexAttribArray(8);
  glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
  glVertexAttribDivisor(8, 1);

  // reset VAO
  glBindVertexArray(0);
}
void InstancedModel::deleteBuffers() { glDeleteBuffers(1, &transformsVBO);
  glDeleteBuffers(1, &positionsVBO);
}
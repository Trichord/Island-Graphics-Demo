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
  instances.reserve(numPositions);
  loadPositions(positionsPath, numPositions);
  generateInstanceMatrices(minSize, maxSize);
  generateWorldAABBs();
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
  generateInstanceMatrices(minSize, maxSize);
  generateWorldAABBs();
  setupInstancedBuffers();
}

InstancedModel::InstancedModel(InstancedModel&& other) noexcept
    : Model{std::move(other)} {
  instancesVBO = other.instancesVBO;
  instances = std::move(other.instances);

  other.instancesVBO = 0;
}

InstancedModel::~InstancedModel() { deleteBuffers(); }

void InstancedModel::drawPrimitive(const Primitive& primitive) {
  glDrawElementsInstanced(GL_TRIANGLES, primitive.getIndicesSize(),
                          GL_UNSIGNED_INT, 0,
                          static_cast<unsigned int>(instances.size()));
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

  while (positionsFile >> x >> y >> z) {
    instances.emplace_back();
    instances.back().position = glm::vec3{x, y, z};
  }

  if (instances.size() != numPositions && numPositions != 0)
    std::cout << positionsPath << " expected " << numPositions << " allocated "
              << instances.size() << std::endl;
}

void InstancedModel::generateInstanceMatrices(float minSize, float maxSize) {
  std::random_device rd;
  std::mt19937 mt(rd());

  // Rotate the model somewhere in the 360 degree range
  std::uniform_real_distribution<float> randRotation{0.0f, 360.0f};
  std::uniform_real_distribution<float> randScale{minSize, maxSize};

  for (auto& instance : instances) {
    float instanceRotation{randRotation(mt)};
    float instanceScale{randScale(mt)};

    glm::mat4 modelMatrix{1.0f};
    modelMatrix = glm::translate(modelMatrix, instance.position);
    modelMatrix = glm::rotate(modelMatrix, glm::radians(instanceRotation),
                              glm::vec3{0.0f, 1.0f, 0.0f});
    modelMatrix = glm::scale(modelMatrix, glm::vec3{instanceScale});

    glm::mat4 normalMatrix{
        glm::transpose(glm::inverse(glm::mat3{modelMatrix}))};

    instance.modelMatrix = modelMatrix;
    instance.normalMatrix = normalMatrix;
  }
}

void InstancedModel::generateWorldAABBs() {
  for (auto& instance : instances) {
    const glm::vec3& localMin{aabb.getMin()};
    const glm::vec3& localMax{aabb.getMax()};
    glm::vec3 localCenter = 0.5f * (localMin + localMax);
    glm::vec3 localExtent = 0.5f * (localMax - localMin);

    glm::mat3 RS{glm::mat3{instance.modelMatrix}};
    glm::mat3 absRS{glm::abs(RS[0]), glm::abs(RS[1]), glm::abs(RS[2])};

    glm::vec3 worldCenter{instance.modelMatrix * glm::vec4(localCenter, 1.0f)};
    glm::vec3 worldExtent{absRS * localExtent};

    instance.worldAABB =
        AABB{worldCenter - worldExtent, worldCenter + worldExtent};
  }
}

void InstancedModel::setupInstancedBuffers() {
  glCreateBuffers(1, &instancesVBO);
  glNamedBufferStorage(instancesVBO, instances.size() * sizeof(InstanceData),
                       instances.data(), GL_DYNAMIC_STORAGE_BIT);

  for (const Primitive::Mesh& mesh : meshes) {
    for (const Primitive& primitive : mesh.primitives) {
      GLuint VAO{primitive.getVAO()};

      // Instanced attributes
      for (GLuint attribIndex : {4, 5, 6, 7, 8, 9, 10, 11})
        glEnableVertexArrayAttrib(VAO, attribIndex);

      // Bind instance VBO to primitive VAO
      glVertexArrayVertexBuffer(VAO, 1, instancesVBO, 0, sizeof(InstanceData));

      // Model matrices layout
      glVertexArrayAttribFormat(VAO, 4, 4, GL_FLOAT, GL_FALSE,
                                offsetof(InstanceData, modelMatrix));
      glVertexArrayAttribFormat(
          VAO, 5, 4, GL_FLOAT, GL_FALSE,
          offsetof(InstanceData, modelMatrix) + sizeof(glm::vec4));
      glVertexArrayAttribFormat(
          VAO, 6, 4, GL_FLOAT, GL_FALSE,
          offsetof(InstanceData, modelMatrix) + 2 * sizeof(glm::vec4));
      glVertexArrayAttribFormat(
          VAO, 7, 4, GL_FLOAT, GL_FALSE,
          offsetof(InstanceData, modelMatrix) + 3 * sizeof(glm::vec4));

      // Normal matrices layout
      glVertexArrayAttribFormat(VAO, 8, 4, GL_FLOAT, GL_FALSE,
                                offsetof(InstanceData, normalMatrix));
      glVertexArrayAttribFormat(
          VAO, 9, 4, GL_FLOAT, GL_FALSE,
          offsetof(InstanceData, normalMatrix) + sizeof(glm::vec4));
      glVertexArrayAttribFormat(
          VAO, 10, 4, GL_FLOAT, GL_FALSE,
          offsetof(InstanceData, normalMatrix) + 2 * sizeof(glm::vec4));
      glVertexArrayAttribFormat(
          VAO, 11, 4, GL_FLOAT, GL_FALSE,
          offsetof(InstanceData, normalMatrix) + 3 * sizeof(glm::vec4));

      // Transform matrices
      glVertexArrayAttribBinding(VAO, 4, 1);
      glVertexArrayAttribBinding(VAO, 5, 1);
      glVertexArrayAttribBinding(VAO, 6, 1);
      glVertexArrayAttribBinding(VAO, 7, 1);

      // Normal matrices
      glVertexArrayAttribBinding(VAO, 8, 1);
      glVertexArrayAttribBinding(VAO, 9, 1);
      glVertexArrayAttribBinding(VAO, 10, 1);
      glVertexArrayAttribBinding(VAO, 11, 1);

      // Divisor
      glVertexArrayBindingDivisor(VAO, 1, 1);
    }
  }
}

void InstancedModel::deleteBuffers() { glDeleteBuffers(1, &instancesVBO); }

bool InstancedModel::performCulling(
    const std::array<glm::vec4, 6>& frustumPlanes, const glm::mat4& modelMatrix,
    const glm::vec3& cameraPos) {
  bool visible{false};

  for (auto& instance : instances) {
    if (instance.isVisible =
            instance.worldAABB.intersectsWorldSpace(frustumPlanes)) {
      double xDist{instance.position.x - cameraPos.x};
      double zDist{instance.position.z - cameraPos.z};

      instance.sortKey = xDist * xDist + zDist * zDist;

      visible = true;
    }
  }

  if (visible) {

  }

  return visible;
}
#include "instanced_model.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <utility>

#include "glad/glad.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "model.h"
#include "primitive.h"
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
  instances = std::move(other.instances);
  visibleInstances = std::move(other.visibleInstances);
  instancesVBO = other.instancesVBO;

  other.instancesVBO = 0;
}

InstancedModel::~InstancedModel() { deleteBuffers(); }

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

  visibleInstances = instances.size();
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
    glm::mat3 RS{glm::mat3{instance.modelMatrix}};
    glm::mat3 absRS{glm::abs(RS[0]), glm::abs(RS[1]), glm::abs(RS[2])};

    glm::vec3 worldCenter{instance.modelMatrix *
                          glm::vec4(aabb.getCenter(), 1.0f)};
    glm::vec3 worldExtent{absRS * aabb.getExtent()};

    instance.worldAABB =
        AABB{worldCenter - worldExtent, worldCenter + worldExtent};
  }
}

void InstancedModel::setupInstancedBuffers() {
  glCreateBuffers(1, &instancesVBO);
  glNamedBufferStorage(
      instancesVBO, instances.size() * sizeof(InstanceData), instances.data(),
      GL_DYNAMIC_STORAGE_BIT | GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT);

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

// Sorts visible instances, updating count if necessary
void InstancedModel::sortInstances() {
  // Move visible instances to front of vector
  auto iter = std::partition(
      instances.begin(), instances.end(),
      [](const InstanceData& instance) { return instance.isVisible; });
  visibleInstances = std::distance(instances.begin(), iter);

  if (!visibleInstances) return;

  // Sort front to back for speed on depth testing, over proper alpha blending
  std::sort(instances.begin(), instances.begin() + visibleInstances,
            [](const InstanceData& a, const InstanceData& b) {
              return a.sortKey < b.sortKey;
            });

  // Update GPU side
  void* bufferPtr{glMapNamedBufferRange(
      instancesVBO, 0, visibleInstances * sizeof(InstanceData),
      GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT)};

  std::memcpy(bufferPtr, instances.data(),
              visibleInstances * sizeof(InstanceData));

  glUnmapNamedBuffer(instancesVBO);
}

std::span<InstanceData> InstancedModel::getInstanceData() { return instances; }

std::size_t InstancedModel::getInstanceCount() const {
  return visibleInstances;
}

void InstancedModel::resetVisibility() {
  Model::resetVisibility();
  visibleInstances = instances.size();

  for (auto& instance : instances) instance.isVisible = true;
}
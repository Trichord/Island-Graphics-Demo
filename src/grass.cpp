#include "../headers/grass.h"
#include "../headers/shader.h"
#include "../headers/texture_utilities.h"
#include "../headers/image_loader.h"
#include "../headers/object_positions.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <random>
#include <iostream>
#include <fstream>

#define TUFT_DENSITY 3
#define ROTATION_VARIATION 10.0f
#define SCALE_VARIATION 0.05f
#define BASE_SCALE_MIN 0.3f
#define BASE_SCALE_MAX 0.5f

Grass::Grass(const std::filesystem::path &positionsPath,
             const std::filesystem::path &texturePath,
             unsigned int numPositions) {
  loadTexture(texturePath);
  loadGrassPositions(positionsPath, numPositions);
  generateTransforms();
  setupBuffers();
}

Grass::Grass(Grass &&other) noexcept {
  VAO = other.VAO;
  VBO = other.VBO;
  EBO = other.EBO;
  transformsVBO = other.transformsVBO;
  normalsVBO = other.normalsVBO;
  transforms = std::move(other.transforms);
  normalMatrices = std::move(other.normalMatrices);

  other.VAO = 0;
  other.VBO = 0;
  other.EBO = 0;
  other.transformsVBO = 0;
  other.normalsVBO = 0;
}

Grass &Grass::operator=(Grass &&other) noexcept {
  if (this != &other) {
    deleteBuffers();

    VAO = other.VAO;
    VBO = other.VBO;
    EBO = other.EBO;
    normalsVBO = other.normalsVBO;
    transformsVBO = other.transformsVBO;
    transforms = std::move(other.transforms);
    normalMatrices = std::move(other.normalMatrices);

    other.VAO = 0;
    other.VBO = 0;
    other.EBO = 0;
    other.transformsVBO = 0;
    other.normalsVBO = 0;
  }

  return *this;
}

Grass::~Grass() { deleteBuffers(); }

void Grass::render(Shader &shader) {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textureID);

  glDisable(GL_CULL_FACE);

  glBindVertexArray(VAO);
  glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0,
                          static_cast<unsigned int>(transforms.size()));

  glEnable(GL_CULL_FACE);

  glBindVertexArray(0);
}

void Grass::loadTexture(const std::filesystem::path &texturePath) {
  ImageLoader imageLoader = ImageLoader(texturePath);
  std::vector<ImageData> imageData = imageLoader.getImageData();

  if (imageData.size())
    textureID = texture_utilities::generateTextureID(imageData[0], true);
}

void Grass::setupBuffers() {
  // Quad VAO, EBO
  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);

  glGenBuffers(1, &VBO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices[0], GL_STATIC_DRAW);

  glGenBuffers(1, &EBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices[0],
               GL_STATIC_DRAW);

  // vertices
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);

  // textures
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                        (void *)(6 * sizeof(float)));

  // Transform VBO
  glGenBuffers(1, &transformsVBO);
  glBindBuffer(GL_ARRAY_BUFFER, transformsVBO);
  glBufferData(GL_ARRAY_BUFFER, transforms.size() * sizeof(glm::mat4),
               transforms.data(), GL_STATIC_DRAW);

  // glm::mat4 instance model matrix
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void *)0);
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                        (void *)(sizeof(glm::vec4)));
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                        (void *)(2 * sizeof(glm::vec4)));
  glEnableVertexAttribArray(5);
  glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                        (void *)(3 * sizeof(glm::vec4)));

  glVertexAttribDivisor(2, 1);
  glVertexAttribDivisor(3, 1);
  glVertexAttribDivisor(4, 1);
  glVertexAttribDivisor(5, 1);

  // Normals VBO
  glGenBuffers(1, &normalsVBO);
  glBindBuffer(GL_ARRAY_BUFFER, normalsVBO);
  glBufferData(GL_ARRAY_BUFFER, normalMatrices.size() * sizeof(glm::mat3),
               normalMatrices.data(), GL_STATIC_DRAW);

  // glm::mat3 instance normal matrix

  glEnableVertexAttribArray(6);
  glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, sizeof(glm::mat3), (void *)0);
  glEnableVertexAttribArray(7);
  glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, sizeof(glm::mat3),
                        (void *)(sizeof(glm::vec3)));
  glEnableVertexAttribArray(8);
  glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, sizeof(glm::mat3),
                        (void *)(2 * sizeof(glm::vec3)));

  glVertexAttribDivisor(6, 1);
  glVertexAttribDivisor(7, 1);
  glVertexAttribDivisor(8, 1);

  // reset VAO
  glBindVertexArray(0);
}

void Grass::generateTransforms() {
  std::random_device rd;
  std::mt19937 mt(rd());
  std::uniform_real_distribution<float> baseRotation(0.0f, 180.0f);
  std::uniform_real_distribution<float> rndRotJitter(-ROTATION_VARIATION,
                                                     ROTATION_VARIATION);
  std::uniform_real_distribution<float> rndScaleJitter(-SCALE_VARIATION,
                                                       SCALE_VARIATION);
  std::uniform_real_distribution<float> rndBaseScale(BASE_SCALE_MIN,
                                                     BASE_SCALE_MAX);

  transforms.reserve(grassPositions.size() * TUFT_DENSITY);
  normalMatrices.reserve(grassPositions.size() * TUFT_DENSITY);

  for (const auto &position : grassPositions) {
    float instanceBaseRotation = baseRotation(mt);
    float instanceScale = rndBaseScale(mt);
    for (unsigned int i = 0; i < TUFT_DENSITY; ++i) {
      float quadAngle = (180.0f / TUFT_DENSITY) * i;

      glm::mat4 transform = glm::mat4(1.0f);
      transform = glm::translate(transform, position);
      transform = glm::rotate(transform, instanceBaseRotation,
                              glm::vec3(0.0f, 1.0f, 0.0f));
      transform =
          glm::rotate(transform, glm::radians(quadAngle + rndRotJitter(mt)),
                      glm::vec3(0.0f, 1.0f, 0.0f));
      transform =
          glm::scale(transform, glm::vec3(instanceScale,
                                          instanceScale + rndScaleJitter(mt),
                                          instanceScale));

      glm::mat3 normalMatrix =
          glm::transpose(glm::inverse(glm::mat3(transform)));

      transforms.emplace_back(std::move(transform));
      normalMatrices.emplace_back(std::move(normalMatrix));
    }
  }
}

void Grass::deleteBuffers() {
  glDeleteBuffers(1, &transformsVBO);
  glDeleteBuffers(1, &normalsVBO);
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &EBO);
  glDeleteVertexArrays(1, &VAO);
}

void Grass::loadGrassPositions(const std::filesystem::path &positionsPath,
                               unsigned int numPositions) {
  grassPositions.reserve(numPositions);

  std::fstream grassFile(positionsPath);
  float x = 0.0f, y = 0.0f, z = 0.0f;

  while (grassFile >> x >> y >> z) grassPositions.emplace_back(x, y, z);
}
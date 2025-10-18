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

#define tuftDensity 2
#define rotationVariation 10.0f
#define scaleVariation 0.1f
#define baseScale 0.5f

Grass::Grass(const std::filesystem::path &texturePath) {
  loadTexture(texturePath);
  loadGrassPositions();
  generateTransforms();
  setupBuffers();
}

Grass::Grass(Grass &&other) noexcept {
  VAO = other.VAO;
  VBO = other.VBO;
  EBO = other.EBO;
  transformsVBO = other.transformsVBO;

  other.VAO = 0;
  other.VBO = 0;
  other.EBO = 0;
  other.transformsVBO = 0;
}

Grass &Grass::operator=(Grass &&other) noexcept {
  if (this != &other) {
    deleteBuffers();

    VAO = other.VAO;
    VBO = other.VBO;
    EBO = other.EBO;
    transformsVBO = other.transformsVBO;

    other.VAO = 0;
    other.VBO = 0;
    other.EBO = 0;
    other.transformsVBO = 0;
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
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);

  // textures
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        (void *)(3 * sizeof(float)));

  // Transform VBO
  glGenBuffers(1, &transformsVBO);
  glBindBuffer(GL_ARRAY_BUFFER, transformsVBO);
  glBufferData(GL_ARRAY_BUFFER, transforms.size() * sizeof(glm::mat4),
               transforms.data(), GL_STATIC_DRAW);

  // glm::mat4 attribute
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

  // reset VAO
  glBindVertexArray(0);
}

void Grass::generateTransforms() {
  std::random_device rd;
  std::mt19937 mt(rd());
  std::uniform_real_distribution<float> rndRotation(-rotationVariation,
                                                    rotationVariation);
  std::uniform_real_distribution<float> rndScale(1.0f - scaleVariation,
                                                 1.0f + scaleVariation);

  transforms.reserve(grassPositions.size() * tuftDensity);

  for (const auto &position : grassPositions) {
    for (unsigned int i = 0; i < tuftDensity; ++i) {
      float baseAngle = (180.0f / tuftDensity) * i;

      glm::mat4 transform = glm::mat4(1.0f);
      transform = glm::translate(transform, position);
      transform = glm::rotate(transform, glm::radians(baseAngle + rndRotation(mt)),
                              glm::vec3(0.0f, 1.0f, 0.0f));
      transform = glm::scale(
          transform,
          glm::vec3(baseScale, baseScale + scaleVariation, baseScale));
      transforms.emplace_back(std::move(transform));
    }
  }
}

void Grass::deleteBuffers() {
  glDeleteBuffers(1, &transformsVBO);
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &EBO);
  glDeleteVertexArrays(1, &VAO);
}

void Grass::loadGrassPositions() {
  grassPositions.reserve(7500);
    
  std::fstream grassFile("./res/grass_positions.txt");
  float x = 0.0f, y = 0.0f, z = 0.0f;

  while (grassFile >> x >> y >> z)
    grassPositions.emplace_back(x, y, z);
}
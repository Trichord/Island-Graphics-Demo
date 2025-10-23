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

namespace {
constexpr float rotationVariation{5.0f};
constexpr float scaleVariationFactor{0.25f};
}  // namespace

Grass::Grass(const std::filesystem::path &positionsPath,
             const std::filesystem::path &texturePath, TuftPattern tuftPattern,
             float minSize, float maxSize, unsigned int tuftDensity,
             unsigned int numPositions) {
  loadTexture(texturePath);
  loadGrassPositions(positionsPath, numPositions);

  if (tuftPattern == Grass::RADIAL_PATTERN)
    generateRadialPatternTransforms(minSize, maxSize, tuftDensity);
  else
    generateStarPatternTransforms(minSize, maxSize, tuftDensity);

  setupBuffers(tuftDensity);
}

Grass::Grass(Grass &&other) noexcept {
  VAO = other.VAO;
  VBO = other.VBO;
  EBO = other.EBO;
  transformsVBO = other.transformsVBO;
  transforms = std::move(other.transforms);
  positionsVBO = other.positionsVBO;
  grassPositions = std::move(other.grassPositions);

  other.VAO = 0;
  other.VBO = 0;
  other.EBO = 0;
  other.transformsVBO = 0;
  other.positionsVBO = 0;
}

Grass &Grass::operator=(Grass &&other) noexcept {
  if (this != &other) {
    deleteBuffers();

    VAO = other.VAO;
    VBO = other.VBO;
    EBO = other.EBO;
    transformsVBO = other.transformsVBO;
    transforms = std::move(other.transforms);
    positionsVBO = other.positionsVBO;
    grassPositions = std::move(other.grassPositions);

    other.VAO = 0;
    other.VBO = 0;
    other.EBO = 0;
    other.transformsVBO = 0;
    other.positionsVBO = 0;
  }

  return *this;
}

Grass::~Grass() { deleteBuffers(); }

void Grass::render() {
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
    textureID = texture_utilities::generateTextureID(
        imageData[0], Sampler(GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR,
                              GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE));
}

void Grass::setupBuffers(unsigned int tuftDensity) {
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

  // Center positions of each tuft
  glGenBuffers(1, &positionsVBO);
  glBindBuffer(GL_ARRAY_BUFFER, positionsVBO);
  glBufferData(GL_ARRAY_BUFFER, grassPositions.size() * sizeof(glm::vec3),
               grassPositions.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(6);
  glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void *)0);
  glVertexAttribDivisor(6, tuftDensity);

  // reset VAO
  glBindVertexArray(0);
}

void Grass::generateRadialPatternTransforms(float minSize, float maxSize,
                                            unsigned int tuftDensity) {
  constexpr float outwardPush{0.04f};
  std::random_device rd;
  std::mt19937 mt(rd());

  // Max range of jitter for scale
  float scaleVariation = (maxSize - minSize) * scaleVariationFactor;

  // Rotate the entire tuft somewhere in the [0-360) degree range
  std::uniform_real_distribution<float> baseRotation(0.0f, 360.0f);

  // Add a tiny bit of rotation jitter for each quad
  std::uniform_real_distribution<float> rndRotJitter(-rotationVariation,
                                                     rotationVariation);

  // Random amount of scale jitter to be applied for each quad
  std::uniform_real_distribution<float> rndScaleJitter(-scaleVariation,
                                                       scaleVariation);

  // Pick a base quad size somewhere between (min, max)
  std::uniform_real_distribution<float> rndBaseScale(minSize, maxSize);

  transforms.reserve(grassPositions.size() * tuftDensity);

  for (const auto &position : grassPositions) {
    // Generate our base rotation and scale
    float instanceBaseRotation = baseRotation(mt);
    float instanceScale = rndBaseScale(mt);
    for (unsigned int i = 0; i < tuftDensity; ++i) {
      // Final quad angle
      float quadAngle =
          (360.0f / tuftDensity) * i + instanceBaseRotation + rndRotJitter(mt);

      // Push it out from the center
      float quadAngleInRad = glm::radians(quadAngle);
      glm::vec3 outwardTranslation =
          glm::vec3(glm::sin(quadAngleInRad), 0.0f, glm::cos(quadAngleInRad)) *
          outwardPush;

      // Scale first, rotate around world origin, then move into position
      glm::mat4 transform = glm::mat4(1.0f);
      transform = glm::translate(transform, position + outwardTranslation);
      transform = glm::rotate(transform, glm::radians(quadAngle),
                              glm::vec3(0.0f, 1.0f, 0.0f));
      transform =
          glm::scale(transform, glm::vec3(instanceScale,
                                          instanceScale + rndScaleJitter(mt),
                                          instanceScale));

      transforms.emplace_back(std::move(transform));
    }
  }
}

void Grass::generateStarPatternTransforms(float minSize, float maxSize,
                                          unsigned int tuftDensity) {
  std::random_device rd;
  std::mt19937 mt(rd());

  // Max variation on base scale
  float scaleVariation = (maxSize - minSize) * scaleVariationFactor;

  // Base rotation of the entire tuft
  std::uniform_real_distribution<float> baseRotation(0.0f, 360.0f);

  // Random amount of rotational jitter
  std::uniform_real_distribution<float> rndRotJitter(-rotationVariation,
                                                     rotationVariation);

  // Random scale jitter based off max variation
  std::uniform_real_distribution<float> rndScaleJitter(-scaleVariation,
                                                       scaleVariation);

  // Random base size between (min, max)
  std::uniform_real_distribution<float> rndBaseScale(minSize, maxSize);

  transforms.reserve(grassPositions.size() * tuftDensity);

  for (const auto &position : grassPositions) {
    // Per-tuft size and scale
    float instanceBaseRotation = baseRotation(mt);
    float instanceScale = rndBaseScale(mt);
    for (unsigned int i = 0; i < tuftDensity; ++i) {
      float quadAngle =
          (360.0f / tuftDensity) * i + instanceBaseRotation + rndRotJitter(mt);

      glm::mat4 transform = glm::mat4(1.0f);
      transform = glm::translate(transform, position);
      transform = glm::rotate(transform, glm::radians(quadAngle),
                              glm::vec3(0.0f, 1.0f, 0.0f));
      transform =
          glm::scale(transform, glm::vec3(instanceScale,
                                          instanceScale + rndScaleJitter(mt),
                                          instanceScale));

      transforms.emplace_back(std::move(transform));
    }
  }
}

void Grass::deleteBuffers() {
  glDeleteBuffers(1, &transformsVBO);
  glDeleteBuffers(1, &positionsVBO);
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &EBO);
  glDeleteVertexArrays(1, &VAO);
}

void Grass::loadGrassPositions(const std::filesystem::path &positionsPath,
                               unsigned int numPositions) {
  grassPositions.reserve(numPositions);
  float x = 0.0f, y = 0.0f, z = 0.0f;

  std::fstream grassFile(positionsPath);

  if (grassFile.is_open())
    while (grassFile >> x >> y >> z) grassPositions.emplace_back(x, y, z);

  else
    std::cerr << "Unable to open " << positionsPath << std::endl;
}
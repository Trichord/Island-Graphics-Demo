#include "quad.h"

#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <random>
#include <vector>

#include "image_loader.h"
#include "texture_utilities.h"
#include "texture_manager.h"

constexpr float rotationVariation{5.0f};
constexpr float scaleVariationFactor{0.25f};

Quad::Quad(TextureManager &textureManager,
           const std::filesystem::path &texturePath,
           const std::filesystem::path &positionsPath, QuadPattern pattern,
           float minSize, float maxSize, unsigned int density,
           unsigned int numPositions)
    : texManager{textureManager},
      handle{textureManager.assignModelHandle()},
      textureSet{textureManager.acquireUniqueTextureSet(handle)} {
  normalMatrices.reserve(numPositions * density);
  positions.reserve(numPositions);
  loadPositions(positionsPath, numPositions);

  if (pattern == Quad::RADIAL_PATTERN)
    generateRadialPatternTransforms(minSize, maxSize, density);
  else
    generateStarPatternTransforms(minSize, maxSize, density);

  setupBuffers(density);
  texManager.queueBaseImageTextureSet(handle, texturePath);
}

Quad::Quad(TextureManager &textureManager, const Quad &borrowTarget,
           const std::filesystem::path &texturePath,
           const std::filesystem::path &positionsPath, QuadPattern pattern,
           float minSize, float maxSize, unsigned int density,
           unsigned int numPositions)
    : texManager{textureManager},
      handle{borrowTarget.handle},
      textureSet{textureManager.borrowTextureSet(borrowTarget.handle)} {
  loadPositions(positionsPath, numPositions);

  if (pattern == Quad::RADIAL_PATTERN)
    generateRadialPatternTransforms(minSize, maxSize, density);
  else
    generateStarPatternTransforms(minSize, maxSize, density);

  setupBuffers(density);
  texManager.queueBaseImageTextureSet(handle, texturePath);
}

Quad::Quad(Quad &&other) noexcept
    : texManager{other.texManager},
      handle{other.handle},
      textureSet{other.textureSet} {
  VAO = other.VAO;
  VBO = other.VBO;
  EBO = other.EBO;
  transformsVBO = other.transformsVBO;
  transforms = std::move(other.transforms);
  positionsVBO = other.positionsVBO;
  positions = std::move(other.positions);
  normalMatricesVBO = other.normalMatricesVBO;
  normalMatrices = std::move(other.normalMatrices);

  other.VAO = 0;
  other.VBO = 0;
  other.EBO = 0;
  other.transformsVBO = 0;
  other.positionsVBO = 0;
  other.normalMatricesVBO = 0;
  other.handle = 0;
}

Quad::~Quad() {
  deleteBuffers();
  if (handle) texManager.releaseModel(handle);
}

void Quad::render() {
  glDisable(GL_CULL_FACE);
  glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);

  if (!textureSet.empty()) glBindTextureUnit(0, textureSet[0].albedo);

  glBindVertexArray(VAO);
  glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0,
                          static_cast<unsigned int>(transforms.size()));

  glEnable(GL_CULL_FACE);
  glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);

  glBindVertexArray(0);
}

/**
* 
*/
void Quad::setupBuffers(unsigned int density) {
  glCreateVertexArrays(1, &VAO);

  // Attribute indices. NOTE: location 2 is not used (for tangents) to
  // maintain shader compatibility between quad and instanced model.
  for (GLuint attribIndex : {0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12})
    glEnableVertexArrayAttrib(VAO, attribIndex);
 
  glCreateBuffers(1, &VBO);
  glNamedBufferStorage(VBO, sizeof(vertices), &vertices[0], 0);

  glCreateBuffers(1, &EBO);
  glNamedBufferStorage(EBO, sizeof(indices), &indices[0], 0);

  glCreateBuffers(1, &transformsVBO);
  glNamedBufferStorage(transformsVBO, transforms.size() * sizeof(glm::mat4),
                       transforms.data(), GL_DYNAMIC_STORAGE_BIT);

  glCreateBuffers(1, &normalMatricesVBO);
  glNamedBufferStorage(normalMatricesVBO,
                       normalMatrices.size() * sizeof(glm::mat4),
                       normalMatrices.data(), GL_DYNAMIC_STORAGE_BIT);

  glCreateBuffers(1, &positionsVBO);
  glNamedBufferStorage(positionsVBO, positions.size() * sizeof(glm::vec3),
                       positions.data(), GL_DYNAMIC_STORAGE_BIT);

  glVertexArrayVertexBuffer(VAO, 0, VBO, 0, 8 * sizeof(float));
  glVertexArrayVertexBuffer(VAO, 1, transformsVBO, 0, sizeof(glm::mat4));
  glVertexArrayVertexBuffer(VAO, 2, normalMatricesVBO, 0, sizeof(glm::mat4));
  glVertexArrayVertexBuffer(VAO, 3, positionsVBO, 0, sizeof(glm::vec3));

  glVertexArrayElementBuffer(VAO, EBO);

  // Vertex layout
  glVertexArrayAttribFormat(VAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
  glVertexArrayAttribFormat(VAO, 1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
  glVertexArrayAttribFormat(VAO, 3, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float));

  // Transform matrices layout
  glVertexArrayAttribFormat(VAO, 4, 4, GL_FLOAT, GL_FALSE, 0);
  glVertexArrayAttribFormat(VAO, 5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4));
  glVertexArrayAttribFormat(VAO, 6, 4, GL_FLOAT, GL_FALSE,
                            2 * sizeof(glm::vec4));
  glVertexArrayAttribFormat(VAO, 7, 4, GL_FLOAT, GL_FALSE,
                            3 * sizeof(glm::vec4));

  // Normal matrices layout
  glVertexArrayAttribFormat(VAO, 8, 4, GL_FLOAT, GL_FALSE, 0);
  glVertexArrayAttribFormat(VAO, 9, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4));
  glVertexArrayAttribFormat(VAO, 10, 4, GL_FLOAT, GL_FALSE,
                            2 * sizeof(glm::vec4));
  glVertexArrayAttribFormat(VAO, 11, 4, GL_FLOAT, GL_FALSE,
                            3 * sizeof(glm::vec4));

  // Center positions layout
  glVertexArrayAttribFormat(VAO, 12, 3, GL_FLOAT, GL_FALSE, 0);

  // Vertex
  glVertexArrayAttribBinding(VAO, 0, 0);
  glVertexArrayAttribBinding(VAO, 1, 0);
  glVertexArrayAttribBinding(VAO, 3, 0);

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

  // Center positions
  glVertexArrayAttribBinding(VAO, 12, 3);

  // Divisors (each cluster shares a center point)
  glVertexArrayBindingDivisor(VAO, 0, 0);
  glVertexArrayBindingDivisor(VAO, 1, 1);
  glVertexArrayBindingDivisor(VAO, 2, 1);
  glVertexArrayBindingDivisor(VAO, 3, density);
}

void Quad::generateRadialPatternTransforms(float minSize, float maxSize,
                                           unsigned int density) {
  constexpr float outwardPush{0.04f};
  std::random_device rd;
  std::mt19937 mt(rd());

  // Max range of jitter for scale
  float scaleVariation{(maxSize - minSize) * scaleVariationFactor};

  // Rotate the entire group somewhere in the [0-360) degree range
  std::uniform_real_distribution<float> baseRotation{0.0f, 360.0f};

  // Add a tiny bit of rotation jitter for each quad
  std::uniform_real_distribution<float> rndRotJitter{-rotationVariation,
                                                     rotationVariation};

  // Random amount of scale jitter to be applied for each quad
  std::uniform_real_distribution<float> rndScaleJitter{-scaleVariation,
                                                       scaleVariation};

  // Random base quad size somewhere between (min, max)
  std::uniform_real_distribution<float> rndBaseScale{minSize, maxSize};

  transforms.reserve(positions.size() * density);

  for (const auto &position : positions) {
    // Generate our base rotation and scale
    float instanceBaseRotation{baseRotation(mt)};
    float instanceScale{rndBaseScale(mt)};
    for (unsigned int i = 0; i < density; ++i) {
      // Final quad angle
      float quadAngle{(360.0f / density) * i + instanceBaseRotation +
                      rndRotJitter(mt)};

      // Push it out from the center
      float quadAngleInRad{glm::radians(quadAngle)};
      glm::vec3 outwardTranslation{
          glm::vec3{glm::sin(quadAngleInRad), 0.0f, glm::cos(quadAngleInRad)} *
          outwardPush};

      // Scale first, rotate around world origin, then move into position
      glm::mat4 transform{1.0f};
      transform = glm::translate(transform, position + outwardTranslation);
      transform = glm::rotate(transform, glm::radians(quadAngle),
                              glm::vec3{0.0f, 1.0f, 0.0f});
      transform =
          glm::scale(transform, glm::vec3(instanceScale,
                                          instanceScale + rndScaleJitter(mt),
                                          instanceScale));

      glm::mat4 normalMatrix{
          glm::transpose(glm::inverse(glm::mat3{transform}))};

      transforms.emplace_back(std::move(transform));
      normalMatrices.emplace_back(std::move(normalMatrix));
    }
  }
}

void Quad::generateStarPatternTransforms(float minSize, float maxSize,
                                         unsigned int density) {
  std::random_device rd;
  std::mt19937 mt(rd());

  // Max variation on base scale
  float scaleVariation{(maxSize - minSize) * scaleVariationFactor};

  // Base rotation of the entire group
  std::uniform_real_distribution<float> baseRotation{0.0f, 360.0f};

  // Random amount of rotational jitter
  std::uniform_real_distribution<float> rndRotJitter{-rotationVariation,
                                                     rotationVariation};

  // Random scale jitter based off max variation
  std::uniform_real_distribution<float> rndScaleJitter{-scaleVariation,
                                                       scaleVariation};

  // Random base size between (min, max)
  std::uniform_real_distribution<float> rndBaseScale{minSize, maxSize};

  transforms.reserve(positions.size() * density);

  for (const auto &position : positions) {
    // Individual quad size and scale
    float instanceBaseRotation{baseRotation(mt)};
    float instanceScale{rndBaseScale(mt)};

    for (unsigned int i = 0; i < density; ++i) {
      // Final quad angle.  Using 180 degrees to avoid overlap, as anything past
      // 180 is a duplicate angle
      float quadAngle{(180.0f / density) * i + instanceBaseRotation +
                      rndRotJitter(mt)};

      glm::mat4 transform{1.0f};
      transform = glm::translate(transform, position);
      transform = glm::rotate(transform, glm::radians(quadAngle),
                              glm::vec3(0.0f, 1.0f, 0.0f));
      transform =
          glm::scale(transform, glm::vec3(instanceScale,
                                          instanceScale + rndScaleJitter(mt),
                                          instanceScale));

      glm::mat4 normalMatrix{
          glm::transpose(glm::inverse(glm::mat3{transform}))};

      transforms.emplace_back(std::move(transform));
      normalMatrices.emplace_back(std::move(normalMatrix));
    }
  }
}

void Quad::deleteBuffers() {
  glDeleteBuffers(1, &transformsVBO);
  glDeleteBuffers(1, &positionsVBO);
  glDeleteBuffers(1, &normalMatricesVBO);
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &EBO);
  glDeleteVertexArrays(1, &VAO);
}

void Quad::loadPositions(const std::filesystem::path &positionsPath,
                         unsigned int numPositions) {
  float x{0.0f};
  float y{0.0f};
  float z{0.0f};

  std::fstream grassFile{positionsPath};

  if (!grassFile.is_open()) {
    std::cerr << "Unable to open " << positionsPath << std::endl;
    return;
  }

  while (grassFile >> x >> y >> z) positions.emplace_back(x, y, z);

  if (positions.size() != numPositions && numPositions != 0)
    std::cout << positionsPath << " expected " << numPositions << " allocated "
              << positions.size() << std::endl;
}

ModelHandle Quad::getModelHandle() { return handle; }
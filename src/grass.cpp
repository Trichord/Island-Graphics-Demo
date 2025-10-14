#include "../headers/grass.h"
#include "../headers/shader.h"
#include "../headers/texture_utilities.h"
#include "../headers/image_loader.h"
#include "../headers/object_positions.h"
#include <glm/glm.hpp>
#include <vector>

Grass::Grass(std::filesystem::path &texturePath) {
  loadTexture(texturePath);
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

void Grass::render(Shader &shader) {}

void Grass::loadTexture(std::filesystem::path &texturePath) {
  ImageLoader imageLoader = ImageLoader(texturePath);
  std::vector<ImageData> imageData = imageLoader.getImageData();

  if (imageData.size())
    textureID = texture_utilities::generateTextureID(imageData[0], false);
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
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void *)0);
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                        (void *)(sizeof(glm::vec4)));
  glEnableVertexAttribArray(5);
  glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                        (void *)(2 * sizeof(glm::vec4)));
  glEnableVertexAttribArray(6);
  glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                        (void *)(3 * sizeof(glm::vec4)));

  glVertexAttribDivisor(3, 1);
  glVertexAttribDivisor(4, 1);
  glVertexAttribDivisor(5, 1);
  glVertexAttribDivisor(6, 1);

  // reset VAO
  glBindVertexArray(0);
}

void Grass::generateTransforms() {
  transforms.reserve(GRASS_POSITIONS.size());
  
  for (const auto &position : GRASS_POSITIONS) {
    
  }
}

void Grass::deleteBuffers() {
  glDeleteBuffers(1, &transformsVBO);
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &EBO);
  glDeleteVertexArrays(1, &VAO);
}
#include "primitive.h"

#include <glad/glad.h>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

// Don't forget to call std::move when calling Primitive constructor!  Avoid
// copies
Primitive::Primitive(std::vector<Vertex>&& verts,
                     std::vector<std::uint32_t>&& inds, int matIndex)
    : materialIndex{matIndex},
      vertices{std::move(verts)},
      indices{std::move(inds)} {
  setupPrimitiveBuffers();
}

Primitive::Primitive(Primitive&& other) noexcept
    : vertices{std::move(other.vertices)}, indices{std::move(other.indices)} {
  VAO = other.VAO;
  VBO = other.VBO;
  EBO = other.EBO;
  materialIndex = other.materialIndex;

  other.EBO = 0;
  other.VBO = 0;
  other.VAO = 0;
}

Primitive& Primitive::operator=(Primitive&& other) noexcept {
  if (this != &other) {
    glDeleteBuffers(1, &EBO);
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);

    vertices = std::move(other.vertices);
    indices = std::move(other.indices);
    VAO = other.VAO;
    VBO = other.VBO;
    EBO = other.EBO;
    materialIndex = other.materialIndex;

    other.EBO = 0;
    other.VBO = 0;
    other.VAO = 0;
  }
  return *this;
}

// Free GPU resources on mesh destruction
Primitive::~Primitive() {
  glDeleteBuffers(1, &EBO);
  glDeleteBuffers(1, &VBO);
  glDeleteVertexArrays(1, &VAO);
}

GLuint Primitive::getVAO() const { return VAO; }

GLsizei Primitive::getIndicesSize() const {
  return static_cast<GLsizei>(indices.size());
}

int Primitive::getMaterialIndex() const { return materialIndex; }

void Primitive::setupPrimitiveBuffers() {
  if (!vertices.size() || !indices.size()) {
    std::cout << "Error: missing either vertices or indices in mesh"
              << std::endl;
    return;
  }

  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);

  glGenBuffers(1, &VBO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0],
               GL_STATIC_DRAW);

  glGenBuffers(1, &EBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               &indices[0], GL_STATIC_DRAW);

  // vertices
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

  // normals
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void*)offsetof(Vertex, normal));

  // tangent
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void*)offsetof(Vertex, tangent));

  // textures
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void*)offsetof(Vertex, texCoords));

  // reset VAO
  glBindVertexArray(0);
}
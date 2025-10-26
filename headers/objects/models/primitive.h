#pragma once

#include <glad/glad.h>

#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "shader.h"

// Primitive owns its VAO / VBO / EBO.  Copying is disabled to avoid double
// frees or unecessary resource holding
class Primitive {
 public:
  struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 tangent;
    glm::vec2 texCoords;

    Vertex(float px = 0.0f, float py = 0.0f, float pz = 0.0f, float nx = 0.0f,
           float ny = 0.0f, float nz = 0.0f, float tx = 0.0f, float ty = 0.0f,
           float tz = 0.0f, float tw = 0.0f, float u = 0.0f, float v = 0.0f)
        : position(px, py, pz),
          normal(nx, ny, nz),
          tangent(tx, ty, tz, tw),
          texCoords(u, v) {};
  };

  struct Mesh {
    std::vector<Primitive> primitives;
  };

  int materialIndex{-1};

  Primitive() = default;
  Primitive(std::vector<Vertex>&& verts, std::vector<std::uint32_t>&& inds,
            int matIndex = -1);
  Primitive(const Primitive&) = delete;
  Primitive& operator=(const Primitive&) = delete;
  Primitive(Primitive&& other) noexcept;
  Primitive& operator=(Primitive&& other) noexcept;
  ~Primitive();

  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;

  void draw() const;

 private:
  GLuint VAO{0};
  GLuint VBO{0};
  GLuint EBO{0};

  void setupPrimitiveBuffers();
};
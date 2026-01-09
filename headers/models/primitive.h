#pragma once

#include <cstdint>
#include <vector>

#include "fastgltf/types.hpp"
#include "glad/glad.h"
#include "glm/glm.hpp"

/* Primitive owns its VAO / VBO / EBO. Copying is disabled to avoid double frees
 * or unecessary resource holding */

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

  Primitive() = default;
  Primitive(std::vector<Vertex>&& verticies,
            std::vector<std::uint32_t>&& indicies, bool hasMaterials = false,
            std::size_t matIndex = 0,
            fastgltf::AlphaMode alphaMode = fastgltf::AlphaMode::Opaque);
  Primitive(const Primitive&) = delete;
  Primitive& operator=(const Primitive&) = delete;
  Primitive(Primitive&& other) noexcept;
  Primitive& operator=(Primitive&& other) noexcept;
  ~Primitive();

  GLuint getVAO() const;
  GLsizei getIndicesSize() const;
  std::size_t getMaterialIndex() const;
  fastgltf::AlphaMode getAlphaMode() const;
  void setMaterialIndex(std::size_t newIndex);
  bool materialsExist() const;

 private:
  GLuint VAO{0};
  GLuint VBO{0};
  GLuint EBO{0};
  bool hasMaterials{false};
  std::size_t materialIndex{0};
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;
  fastgltf::AlphaMode alphaMode;

  void setupPrimitiveBuffers();
};
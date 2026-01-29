#pragma once

#include <cstdint>
#include <vector>

#include "aabb.h"
#include "fastgltf/types.hpp"
#include "glad/glad.h"
#include "glm/glm.hpp"

/* Primitive owns its VAO / VBO / EBO. Copying is disabled to avoid double frees
 * or unecessary resource holding */

class Primitive {
 public:
  struct Vertex {
    glm::vec3 position{1.0f};
    glm::vec3 normal{1.0f};
    glm::vec4 tangent{1.0f};
    glm::vec2 texCoords{1.0f};
  };

  struct Mesh {
    std::vector<Primitive> primitives;
    bool isVisible{true};
  };

  Primitive() = default;
  Primitive(std::vector<Vertex>&& vertices,
            std::vector<std::uint32_t>&& indices, AABB&& aabb,
            bool hasMaterials = false, std::size_t matIndex = 0,
            fastgltf::AlphaMode alphaMode = fastgltf::AlphaMode::Opaque);
  Primitive(const Primitive&) = delete;
  Primitive& operator=(const Primitive&) = delete;
  Primitive(Primitive&& other) noexcept;
  Primitive& operator=(Primitive&& other) noexcept;
  ~Primitive();

  GLuint getVAO() const;
  const AABB& getAABB() const;
  GLsizei getIndicesSize() const;
  std::size_t getMaterialIndex() const;
  fastgltf::AlphaMode getAlphaMode() const;
  const std::vector<Vertex>& getVertices() const;
  void setMaterialIndex(std::size_t newIndex);
  bool materialsExist() const;

 private:
  GLuint VAO{0};
  GLuint VBO{0};
  GLuint EBO{0};
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;
  AABB aabb;
  bool hasMaterials{false};
  std::size_t materialIndex{0};
  fastgltf::AlphaMode alphaMode{fastgltf::AlphaMode::Opaque};

  void setupPrimitiveBuffers();
};
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "aabb.h"
#include "fastgltf/types.hpp"
#include "glad/glad.h"
#include "glm/glm.hpp"

class Primitive
{
  public:
    struct Vertex
    {
        glm::vec3 position{0.0f};
        glm::vec3 normal{0.0f};
        glm::vec4 tangent{0.0f};
        glm::vec2 texCoords{0.0f};
    };

    struct Mesh
    {
        std::vector<Primitive> primitives;
        bool isVisible{true};
    };

    Primitive() = default;
    Primitive(std::vector<Vertex> &&vertices, std::vector<std::uint32_t> &&indices, AABB &&aabb,
              bool hasMaterials = false, std::size_t matIndex = 0,
              fastgltf::AlphaMode alphaMode = fastgltf::AlphaMode::Opaque);

    GLuint getVAOIndex() const;
    GLuint getEBOIndex() const;
    const AABB &getAABB() const;
    fastgltf::AlphaMode getAlphaMode() const;
    std::span<const Vertex> getVertices() const;
    std::span<const std::uint32_t> getIndices() const;

    void setVAOIndex(GLuint index);
    void setEBOIndex(GLuint index);
    void setAABB(const AABB &otherAABB);

    bool materialsExist() const;
    std::size_t getMaterialIndex() const;
    void setMaterialIndex(std::size_t newIndex);

    std::span<Vertex> getMutableVertices();

  private:
    std::vector<Vertex> vertices_;
    std::vector<std::uint32_t> indices_;

    GLuint VAOIndex_{0};
    GLuint EBOIndex_{0};

    AABB aabb_;
    bool hasMaterials_{false};
    std::size_t materialIndex_{0};
    fastgltf::AlphaMode alphaMode_{fastgltf::AlphaMode::Opaque};
};
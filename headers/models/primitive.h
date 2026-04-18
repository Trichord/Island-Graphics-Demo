#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "aabb.h"
#include "fastgltf/types.hpp"
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
              std::size_t matIndex = 0, fastgltf::AlphaMode alphaMode = fastgltf::AlphaMode::Opaque);

    int getVAOIndex() const;
    int getEBOIndex() const;
    const AABB &getAABB() const;
    int getMetadataBufferIndex() const;
    int getMatricesBufferIndex() const;
    fastgltf::AlphaMode getAlphaMode() const;
    std::span<const Vertex> getVertices() const;
    std::span<const std::uint32_t> getIndices() const;

    void setVAOIndex(int index);
    void setEBOIndex(int index);
    void setAABB(const AABB &otherAABB);
    void setMetadataBufferIndex(int index);
    void setMatricesBufferIndex(int index);

    std::size_t getMaterialIndex() const;
    std::span<Vertex> getMutableVertices();

  private:
    std::vector<Vertex> vertices_;
    std::vector<std::uint32_t> indices_;

    int VAOIndex_{0};
    int EBOIndex_{0};
    int materialBufferIndex_{0};
    int matricesBufferIndex_{0};

    AABB aabb_;
    std::size_t materialIndex_{0};
    fastgltf::AlphaMode alphaMode_{fastgltf::AlphaMode::Opaque};
};
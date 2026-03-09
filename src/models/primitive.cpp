#include "primitive.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <utility>
#include <vector>

#include "aabb.h"
#include "fastgltf/types.hpp"
#include "glad/glad.h"

Primitive::Primitive(std::vector<Vertex> &&vertices_, std::vector<std::uint32_t> &&indices_, AABB &&aabb_,
                     bool hasMaterials_, std::size_t materialIndex_, fastgltf::AlphaMode alphaMode_)
    : vertices_{std::move(vertices_)}, indices_{std::move(indices_)}, aabb_{aabb_}, hasMaterials_{hasMaterials_},
      materialIndex_{materialIndex_}, alphaMode_{alphaMode_}
{
}

Primitive::Primitive(Primitive &&other) noexcept
    : vertices_{std::move(other.vertices_)}, indices_{std::move(other.indices_)}
{
    VAO_ = other.VAO_;
    VBO_ = other.VBO_;
    EBO_ = other.EBO_;
    hasMaterials_ = other.hasMaterials_;
    materialIndex_ = other.materialIndex_;
    alphaMode_ = other.alphaMode_;
    aabb_ = other.aabb_;

    other.EBO_ = 0;
    other.VBO_ = 0;
    other.VAO_ = 0;
}

Primitive &Primitive::operator=(Primitive &&other) noexcept
{
    if (this != &other)
    {
        glDeleteBuffers(1, &EBO_);
        glDeleteBuffers(1, &VBO_);
        glDeleteVertexArrays(1, &VAO_);

        vertices_ = std::move(other.vertices_);
        indices_ = std::move(other.indices_);
        VAO_ = other.VAO_;
        VBO_ = other.VBO_;
        EBO_ = other.EBO_;
        hasMaterials_ = other.hasMaterials_;
        materialIndex_ = other.materialIndex_;
        alphaMode_ = other.alphaMode_;

        other.EBO_ = 0;
        other.VBO_ = 0;
        other.VAO_ = 0;
    }
    return *this;
}

// Free GPU resources on mesh destruction
Primitive::~Primitive()
{
    glDeleteBuffers(1, &EBO_);
    glDeleteBuffers(1, &VBO_);
    glDeleteVertexArrays(1, &VAO_);
}

GLuint Primitive::getVAO() const
{
    return VAO_;
}

std::size_t Primitive::getMaterialIndex() const
{
    return materialIndex_;
}

fastgltf::AlphaMode Primitive::getAlphaMode() const
{
    return alphaMode_;
}

const AABB &Primitive::getAABB() const
{
    return aabb_;
}

bool Primitive::materialsExist() const
{
    return hasMaterials_;
}

void Primitive::setMaterialIndex(std::size_t newIndex)
{
    materialIndex_ = newIndex;
    hasMaterials_ = true;
}

/* Vertex attribute layout:
 * 0 - position (vec3)
 * 1 - normal (vec3)
 * 2 - tangent (vec4)
 * 3 - texture coordinates (vec2)
 */
void Primitive::setupPrimitiveBuffers()
{
    if (!vertices_.size() || !indices_.size())
        throw std::runtime_error("ERROR: Primitive missing vertices or indices");

    glCreateVertexArrays(1, &VAO_);

    glEnableVertexArrayAttrib(VAO_, 0);
    glEnableVertexArrayAttrib(VAO_, 1);
    glEnableVertexArrayAttrib(VAO_, 2);
    glEnableVertexArrayAttrib(VAO_, 3);

    glCreateBuffers(1, &VBO_);
    glNamedBufferStorage(VBO_, vertices_.size() * sizeof(Vertex), vertices_.data(), 0);

    glCreateBuffers(1, &EBO_);
    glNamedBufferStorage(EBO_, indices_.size() * sizeof(unsigned int), indices_.data(), 0);

    glVertexArrayVertexBuffer(VAO_, 0, VBO_, 0, sizeof(Vertex));

    glVertexArrayElementBuffer(VAO_, EBO_);

    glVertexArrayAttribFormat(VAO_, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
    glVertexArrayAttribFormat(VAO_, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
    glVertexArrayAttribFormat(VAO_, 2, 4, GL_FLOAT, GL_FALSE, offsetof(Vertex, tangent));
    glVertexArrayAttribFormat(VAO_, 3, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, texCoords));

    glVertexArrayAttribBinding(VAO_, 0, 0);
    glVertexArrayAttribBinding(VAO_, 1, 0);
    glVertexArrayAttribBinding(VAO_, 2, 0);
    glVertexArrayAttribBinding(VAO_, 3, 0);
}

std::span<const Primitive::Vertex> Primitive::getVertices() const
{
    return vertices_;
}

std::span<const std::uint32_t> Primitive::getIndices() const
{
    return indices_;
}

std::span<Primitive::Vertex> Primitive::getMutableVertices()
{
    return vertices_;
}

void Primitive::setAABB(const AABB &otherAABB)
{
    aabb_ = otherAABB;
}
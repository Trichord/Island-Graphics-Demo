#include "primitive.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

#include "aabb.h"
#include "fastgltf/types.hpp"
#include "glad/glad.h"

Primitive::Primitive(std::vector<Vertex> &&vertices_, std::vector<std::uint32_t> &&indices_, AABB &&aabb_,
                     bool hasMaterials_, std::size_t materialIndex_, fastgltf::AlphaMode alphaMode_)
    : vertices{std::move(vertices_)}, indices{std::move(indices_)}, aabb{aabb_}, hasMaterials{hasMaterials_},
      materialIndex{materialIndex_}, alphaMode{alphaMode_}
{
    setupPrimitiveBuffers();
}

Primitive::Primitive(Primitive &&other) noexcept
    : vertices{std::move(other.vertices)}, indices{std::move(other.indices)}
{
    VAO = other.VAO;
    VBO = other.VBO;
    EBO = other.EBO;
    hasMaterials = other.hasMaterials;
    materialIndex = other.materialIndex;
    alphaMode = other.alphaMode;
    aabb = other.aabb;

    other.EBO = 0;
    other.VBO = 0;
    other.VAO = 0;
}

Primitive &Primitive::operator=(Primitive &&other) noexcept
{
    if (this != &other)
    {
        glDeleteBuffers(1, &EBO);
        glDeleteBuffers(1, &VBO);
        glDeleteVertexArrays(1, &VAO);

        vertices = std::move(other.vertices);
        indices = std::move(other.indices);
        VAO = other.VAO;
        VBO = other.VBO;
        EBO = other.EBO;
        hasMaterials = other.hasMaterials;
        materialIndex = other.materialIndex;
        alphaMode = other.alphaMode;

        other.EBO = 0;
        other.VBO = 0;
        other.VAO = 0;
    }
    return *this;
}

// Free GPU resources on mesh destruction
Primitive::~Primitive()
{
    glDeleteBuffers(1, &EBO);
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
}

GLuint Primitive::getVAO() const
{
    return VAO;
}

GLsizei Primitive::getIndicesSize() const
{
    return static_cast<GLsizei>(indices.size());
}

std::size_t Primitive::getMaterialIndex() const
{
    return materialIndex;
}

fastgltf::AlphaMode Primitive::getAlphaMode() const
{
    return alphaMode;
}

const AABB &Primitive::getAABB() const
{
    return aabb;
}

bool Primitive::materialsExist() const
{
    return hasMaterials;
}

void Primitive::setMaterialIndex(std::size_t newIndex)
{
    materialIndex = newIndex;
    hasMaterials = true;
}

/* Vertex attribute layout:
 * 0 - position (vec3)
 * 1 - normal (vec3)
 * 2 - tangent (vec4)
 * 3 - texture coordinates (vec2)
 */
void Primitive::setupPrimitiveBuffers()
{
    if (!vertices.size() || !indices.size())
    {
        std::cout << "Error: missing either vertices or indices in mesh" << std::endl;
        return;
    }

    glCreateVertexArrays(1, &VAO);

    glEnableVertexArrayAttrib(VAO, 0);
    glEnableVertexArrayAttrib(VAO, 1);
    glEnableVertexArrayAttrib(VAO, 2);
    glEnableVertexArrayAttrib(VAO, 3);

    glCreateBuffers(1, &VBO);
    glNamedBufferStorage(VBO, vertices.size() * sizeof(Vertex), vertices.data(), 0);

    glCreateBuffers(1, &EBO);
    glNamedBufferStorage(EBO, indices.size() * sizeof(unsigned int), indices.data(), 0);

    glVertexArrayVertexBuffer(VAO, 0, VBO, 0, sizeof(Vertex));

    glVertexArrayElementBuffer(VAO, EBO);

    glVertexArrayAttribFormat(VAO, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
    glVertexArrayAttribFormat(VAO, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
    glVertexArrayAttribFormat(VAO, 2, 4, GL_FLOAT, GL_FALSE, offsetof(Vertex, tangent));
    glVertexArrayAttribFormat(VAO, 3, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, texCoords));

    glVertexArrayAttribBinding(VAO, 0, 0);
    glVertexArrayAttribBinding(VAO, 1, 0);
    glVertexArrayAttribBinding(VAO, 2, 0);
    glVertexArrayAttribBinding(VAO, 3, 0);
}

const std::vector<Primitive::Vertex> &Primitive::getVertices() const
{
    return vertices;
}
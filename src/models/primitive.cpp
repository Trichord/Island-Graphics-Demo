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

void Primitive::setEBOIndex(GLuint index)
{
    EBOIndex_ = index;
}

void Primitive::setVAOIndex(GLuint index)
{
    VAOIndex_ = index;
}

GLuint Primitive::getEBOIndex() const
{
    return EBOIndex_;
}

GLuint Primitive::getVAOIndex() const
{
    return VAOIndex_;
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
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

Primitive::Primitive(std::vector<Vertex> &&vertices, std::vector<std::uint32_t> &&indices, AABB &&aabb,
                     std::size_t materialIndex, fastgltf::AlphaMode alphaMode)
    : vertices_{std::move(vertices)}, indices_{std::move(indices)}, aabb_{aabb}, materialIndex_{materialIndex},
      alphaMode_{alphaMode}
{
}

fastgltf::AlphaMode Primitive::getAlphaMode() const
{
    return alphaMode_;
}

const AABB &Primitive::getAABB() const
{
    return aabb_;
}

std::size_t Primitive::getMaterialIndex() const
{
    return materialIndex_;
}

void Primitive::setEBOIndex(int index)
{
    EBOIndex_ = index;
}

void Primitive::setVAOIndex(int index)
{
    VAOIndex_ = index;
}

int Primitive::getEBOIndex() const
{
    return EBOIndex_;
}

int Primitive::getVAOIndex() const
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

int Primitive::getMetadataBufferIndex() const
{
    return materialBufferIndex_;
}

void Primitive::setMetadataBufferIndex(int index)
{
    materialBufferIndex_ = index;
}

int Primitive::getMatricesBufferIndex() const
{
    return matricesBufferIndex_;
}

void Primitive::setMatricesBufferIndex(int index)
{
    matricesBufferIndex_ = index;
}
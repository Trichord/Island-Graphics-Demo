#include "fastgltf_loader.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "aabb.h"
#include "fastgltf/core.hpp"
#include "fastgltf/glm_element_traits.hpp"
#include "fastgltf/math.hpp"
#include "fastgltf/tools.hpp"
#include "fastgltf/types.hpp"
#include "glm/glm.hpp"
#include "primitive.h"
#include "textures.h"

std::string fastgltfErrorToString(fastgltf::Error error)
{
    switch (error)
    {
    case fastgltf::Error::None:
        return "No error.";
    case fastgltf::Error::InvalidPath:
        return "Invalid glTF path.";
    case fastgltf::Error::MissingExtensions:
        return "Required extensions not enabled.";
    case fastgltf::Error::UnknownRequiredExtension:
        return "Unsupported required extension.";
    case fastgltf::Error::InvalidJson:
        return "JSON parsing error.";
    case fastgltf::Error::InvalidGltf:
        return "Invalid or incomplete glTF structure.";
    case fastgltf::Error::InvalidOrMissingAssetField:
        return "Missing or invalid asset field.";
    case fastgltf::Error::InvalidGLB:
        return "Invalid GLB container.";
    case fastgltf::Error::MissingField:
        return "Missing field in JSON (internal use).";
    case fastgltf::Error::MissingExternalBuffer:
        return "External buffer not found.";
    case fastgltf::Error::UnsupportedVersion:
        return "Unsupported glTF version.";
    case fastgltf::Error::InvalidURI:
        return "Failed to parse URI.";
    case fastgltf::Error::InvalidFileData:
        return "Invalid file data or unknown file type.";
    case fastgltf::Error::FailedWritingFiles:
        return "Failed to write buffers/images to disk.";
    case fastgltf::Error::FileBufferAllocationFailed:
        return "Failed to allocate buffer for file.";
    default:
        return "Unknown FastGLTF error.";
    }
}

fastgltf::Asset fastgltfLoader::loadAsset(const std::filesystem::path &path)
{
    static fastgltf::Parser parser;
    auto data = fastgltf::GltfDataBuffer::FromPath(path);

    if (data.error() != fastgltf::Error::None)
    {
        std::cerr << "FastGLTF error loading from " << path << std::endl
                  << fastgltfErrorToString(data.error()) << std::endl;
        return fastgltf::Asset{};
    }

    auto assetResult =
        parser.loadGltfJson(data.get(), path.parent_path(),
                            fastgltf::Options::GenerateMeshIndices | fastgltf::Options::LoadExternalBuffers);

    if (auto error = assetResult.error(); error != fastgltf::Error::None)
        std::cerr << "fastgltf error while loading: " << path << std::endl;

    return std::move(assetResult.get());
}

void calculateAABB(std::vector<Primitive> &primitives)
{
    for (auto &primitive : primitives)
    {
        // Create AABB
        float minX{std::numeric_limits<float>::infinity()};
        float minY{std::numeric_limits<float>::infinity()};
        float minZ{std::numeric_limits<float>::infinity()};
        float maxX{-std::numeric_limits<float>::infinity()};
        float maxY{-std::numeric_limits<float>::infinity()};
        float maxZ{-std::numeric_limits<float>::infinity()};

        for (const auto &vertex : primitive.vertices)
        {
            minX = std::min(vertex.position.x, minX);
            minY = std::min(vertex.position.y, minY);
            minZ = std::min(vertex.position.z, minZ);

            maxX = std::max(vertex.position.x, maxX);
            maxY = std::max(vertex.position.y, maxY);
            maxZ = std::max(vertex.position.z, maxZ);
        }

        primitive.aabb = {minX, minY, minZ, maxX, maxY, maxZ};
    }
}

void applyNodeTransforms(const fastgltf::Asset &asset, std::vector<Primitive> &primitives)
{
    for (std::size_t i{0}; i < asset.scenes.size(); ++i)
    {
        fastgltf::iterateSceneNodes(
            asset, i, fastgltf::math::fmat4x4(1.0f), [&](const fastgltf::Node &node, fastgltf::math::fmat4x4 matrix) {
                if (node.meshIndex.has_value())
                {
                    for (auto &primitive : primitives)
                    {
                        glm::mat4 modelMatrix{0.0f};
                        memcpy(&modelMatrix, &matrix, 16 * sizeof(matrix[0][0]));

                        glm::mat3 normalMatrix{glm::transpose(glm::inverse(modelMatrix))};

                        for (Primitive::Vertex &vertex : primitive.vertices)
                        {
                            vertex.position = glm::vec3{modelMatrix * glm::vec4{vertex.position, 1.0f}};
                            vertex.normal = normalMatrix * vertex.normal;
                        }
                    }
                }
            });
    }
}

std::vector<Primitive> fastgltfLoader::loadMeshes(const fastgltf::Asset &asset)
{
    std::vector<Primitive> primitives;
    std::size_t numPrimitives{0};

    for (const auto &gltfMesh : asset.meshes)
    {
        numPrimitives += gltfMesh.primitives.size();
    }

    primitives.reserve(numPrimitives);

    for (const auto &gltfMesh : asset.meshes)
    {
        for (const auto &gltfPrimitive : gltfMesh.primitives)
        {
            std::vector<Primitive::Vertex> vertices;
            std::vector<std::uint32_t> indices;

            auto *positionIt = gltfPrimitive.findAttribute("POSITION");
            auto *normalIt = gltfPrimitive.findAttribute("NORMAL");
            auto *tangentIt = gltfPrimitive.findAttribute("TANGENT");
            auto *texCoordIt = gltfPrimitive.findAttribute("TEXCOORD_0");

            if (!positionIt)
                throw std::runtime_error{"fastGLTF loading error: Model has no position accessor"};

            auto &positionAccessor = asset.accessors[positionIt->accessorIndex];
            vertices.resize(positionAccessor.count);

            if (positionAccessor.type == fastgltf::AccessorType::Vec3)
            {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(
                    asset, positionAccessor,
                    [&](glm::vec3 position, std::size_t index) { vertices[index].position = position; });
            }
#ifdef DEBUG
            else
            {
                std::cerr << "ERROR: Fastgltf loading non-vec3 positions" << std::endl;
            }
#endif // DEBUG

            // Normals
            if (normalIt)
            {
                auto &normalAccessor = asset.accessors[normalIt->accessorIndex];

                if (normalAccessor.type == fastgltf::AccessorType::Vec3)
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(
                        asset, normalAccessor,
                        [&](glm::vec3 normal, std::size_t index) { vertices[index].normal = normal; });
                }
#ifdef DEBUG
                else
                {
                    std::cerr << "ERROR: Fastgltf loading non-vec3 normals" << std::endl;
                }
#endif // DEBUG
            }

            // Tangents
            if (tangentIt)
            {
                auto &tangentAccessor = asset.accessors[tangentIt->accessorIndex];

                if (tangentAccessor.type == fastgltf::AccessorType::Vec4)
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(
                        asset, tangentAccessor,
                        [&](glm::vec4 tangent, std::size_t index) { vertices[index].tangent = tangent; });
                }
#ifdef DEBUG
                else
                {
                    std::cerr << "ERROR: Fastgltf loading non-vec4 tangents" << std::endl;
                }
#endif // DEBUG
            }

            // Texture coordinates
            if (texCoordIt)
            {
                auto &texCoordAccessor = asset.accessors[texCoordIt->accessorIndex];

                if (texCoordAccessor.type == fastgltf::AccessorType::Vec2)
                {

                    fastgltf::iterateAccessorWithIndex<glm::vec2>(
                        asset, texCoordAccessor,
                        [&](glm::vec2 texCoords, std::size_t index) { vertices[index].texCoords = texCoords; });
                }
#ifdef DEBUG
                else
                {
                    std::cerr << "ERROR: Fastgltf loading non-vec2 uv coordinates" << std::endl;
                }
#endif // DEBUG
            }

            // Indices
            if (gltfPrimitive.indicesAccessor.has_value())
            {
                auto &indicesAccessor = asset.accessors[gltfPrimitive.indicesAccessor.value()];
                indices.resize(indicesAccessor.count);

                if (indicesAccessor.type == fastgltf::AccessorType::Scalar)
                {
                    std::size_t idx{0};
                    fastgltf::iterateAccessor<std::uint32_t>(asset, indicesAccessor,
                                                             [&](std::uint32_t index) { indices[idx++] = index; });
                }
#ifdef DEBUG
                else
                {
                    std::cerr << "ERROR: Fastgltf loading non-scalar indices" << std::endl;
                }
#endif // DEBUG
            }

            // Check if there are any materials at all
            std::size_t materialIndex{0};
            fastgltf::AlphaMode alphaMode{fastgltf::AlphaMode::Opaque};

            if (gltfPrimitive.materialIndex.has_value() && !asset.images.empty() && !asset.materials.empty())
            {
                materialIndex = gltfPrimitive.materialIndex.value();
                alphaMode = asset.materials[materialIndex].alphaMode;
            }

            // Build the primitive
            primitives.emplace_back(Primitive{.vertices = std::move(vertices),
                                              .indices = std::move(indices),
                                              .materialIndex = static_cast<int>(materialIndex),
                                              .alphaMode = alphaMode});
        }
    }

    applyNodeTransforms(asset, primitives);
    calculateAABB(primitives);

    return primitives;
}

std::vector<std::filesystem::path> fastgltfLoader::getURIs(const fastgltf::Asset &asset,
                                                           const std::filesystem::path &parentPath)
{
    std::vector<std::filesystem::path> URIpaths;
    URIpaths.reserve(asset.images.size());

    for (int i = 0; i < asset.images.size(); ++i)
    {
        fastgltf::Image image = asset.images[i];

        if (auto *uri = std::get_if<fastgltf::sources::URI>(&image.data))
        {
            std::filesystem::path relativeImagePath(uri->uri.path().begin(), uri->uri.path().end());

            std::filesystem::path fullImagePath = parentPath / relativeImagePath;

            if (!uri->uri.isLocalPath())
                throw std::runtime_error{"URI not local: " + fullImagePath.string()};

            URIpaths.emplace_back(fullImagePath);
        }
    }

    return URIpaths;
}

std::pair<unsigned int, Sampler> fastgltfLoader::getTextureInfo(const fastgltf::Asset &asset,
                                                                const fastgltf::TextureInfo &texInfo)
{
    const auto &texture = asset.textures[texInfo.textureIndex];

    auto magFilter = fastgltf::Filter::Linear;
    auto minFilter = fastgltf::Filter::LinearMipMapLinear;
    auto wrapS = fastgltf::Wrap::Repeat;
    auto wrapT = fastgltf::Wrap::Repeat;

    if (texture.samplerIndex.has_value())
    {
        auto samplerIndex = texture.samplerIndex.value();
        wrapS = asset.samplers[samplerIndex].wrapS;
        wrapT = asset.samplers[samplerIndex].wrapT;

        if (asset.samplers[samplerIndex].magFilter.has_value())
            magFilter = asset.samplers[samplerIndex].magFilter.value();

        if (asset.samplers[samplerIndex].minFilter.has_value())
            minFilter = asset.samplers[samplerIndex].minFilter.value();
    }

    const unsigned int assetImageIndex = static_cast<unsigned int>(texture.imageIndex.value());
    Sampler sampler{static_cast<uint16_t>(magFilter), static_cast<uint16_t>(minFilter), static_cast<uint16_t>(wrapS),
                    static_cast<uint16_t>(wrapT)};

    return {assetImageIndex, sampler};
}
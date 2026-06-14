#define GLM_ENABLE_EXPERIMENTAL

#include "utilities/asset_loader.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/packing.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "models/aabb.h"
#include "models/primitive.h"
#include "models/skeleton.h"
#include "renderer/textures.h"
#include "scene/handle.h"

namespace
{

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

glm::vec4 toGLMVec4(const fastgltf::math::fvec4 &vec)
{
    return glm::vec4{vec.x(), vec.y(), vec.z(), vec.w()};
};

void getNodeTransforms(const fastgltf::Asset &asset, std::vector<Primitive> &primitives)
{
    struct PrimitiveRange
    {
        std::size_t start{0};
        std::size_t count{0};
    };

    std::vector<PrimitiveRange> meshToPrimitive(primitives.size());

    std::size_t offset{0};
    for (std::size_t meshIndex{0}; meshIndex < asset.meshes.size(); ++meshIndex)
    {
        std::size_t numPrimitives{asset.meshes[meshIndex].primitives.size()};
        meshToPrimitive[meshIndex].start = offset;
        meshToPrimitive[meshIndex].count = numPrimitives;

        offset += numPrimitives;
    }

    for (std::size_t i{0}; i < asset.scenes.size(); ++i)
    {
        fastgltf::iterateSceneNodes(
            asset, i, fastgltf::math::fmat4x4(1.0f), [&](const fastgltf::Node &node, fastgltf::math::fmat4x4 matrix) {
                if (node.meshIndex.has_value())
                {
                    const std::size_t meshIndex{node.meshIndex.value()};
                    const PrimitiveRange &range{meshToPrimitive[meshIndex]};
                    const glm::mat4 nodeTransform{toGLMVec4(matrix.col(0)), toGLMVec4(matrix.col(1)),
                                                  toGLMVec4(matrix.col(2)), toGLMVec4(matrix.col(3))};

                    for (std::size_t i{range.start}; i < range.start + range.count; ++i)
                        primitives[i].nodeTransform = nodeTransform;
                }
            });
    }
}

void loadSkin(const fastgltf::Asset &asset, const fastgltf::Skin &skin, Skeleton &skeleton)
{
    for (std::size_t i{0}; i < skin.joints.size(); ++i)
    {
        const std::size_t nodeIndex{skin.joints[i]};
        const auto &node = asset.nodes[nodeIndex];

        if (const auto *trs = std::get_if<fastgltf::TRS>(&node.transform))
        {
            auto uniformScale = [](const glm::vec3 &scale) {
                constexpr float epsilon{0.0001};
                return std::abs(scale.x - scale.y) < epsilon && std::abs(scale.y - scale.z) < epsilon;
            };

            const auto &t = trs->translation;
            glm::vec3 translation{t.x(), t.y(), t.z()};
            skeleton.localTranslations[i] = translation;

            const auto &r = trs->rotation;
            const glm::quat rotation{r.w(), r.x(), r.y(), r.z()};
            skeleton.localRotations[i] = rotation;

            // Only use uniform scale
            const auto &s = trs->scale;
            const glm::vec3 scale{s.x(), s.y(), s.z()};
            if (!uniformScale(scale))
                std::cerr << "WARNING: Non uniform skeleton scale unsupported" << std::endl;

            skeleton.localScales[i] = scale;
        }
        else
        {
            const auto &trsMat4 = std::get<fastgltf::math::fmat4x4>(node.transform);
            const glm::mat4 modelMatrix{toGLMVec4(trsMat4.col(0)), toGLMVec4(trsMat4.col(1)), toGLMVec4(trsMat4.col(2)),
                                        toGLMVec4(trsMat4.col(3))};

            glm::vec3 scale{1.0f};
            glm::vec3 translation{0.0f};
            glm::quat rotation{};
            glm::vec3 skew{0.0f};
            glm::vec4 perspective{0.0f};

            glm::decompose(modelMatrix, scale, rotation, translation, skew, perspective);

            skeleton.localTranslations[i] = translation;
            skeleton.localRotations[i] = rotation;
            skeleton.localScales[i] = scale;
        }
    }

    if (skin.inverseBindMatrices.has_value())
    {
        auto &accessor = asset.accessors[*skin.inverseBindMatrices];

        fastgltf::iterateAccessorWithIndex<glm::mat4>(
            asset, accessor, [&](glm::mat4 mat, std::size_t index) { skeleton.invBindMatrices[index] = mat; });
    }
}

void loadWeightsAndIDs(const fastgltf::Asset &asset, const std::vector<Primitive> &primitives, Skeleton &skeleton)
{
    std::size_t vertexStart{0};
    std::size_t primitiveIndex{0};
    for (const auto &mesh : asset.meshes)
    {
        for (const auto &primitive : mesh.primitives)
        {
            auto *weightIt = primitive.findAttribute("WEIGHTS_0");
            auto *jointIt = primitive.findAttribute("JOINTS_0");

            if (jointIt != primitive.attributes.end())
            {
                auto &accessor = asset.accessors[jointIt->accessorIndex];

                fastgltf::iterateAccessorWithIndex<glm::uvec4>(
                    asset, accessor,
                    [&](glm::uvec4 jointIDs, std::size_t index) { skeleton.boneIDs[vertexStart + index] = jointIDs; });
            }

            if (weightIt != primitive.attributes.end())
            {
                auto &accessor = asset.accessors[weightIt->accessorIndex];

                fastgltf::iterateAccessorWithIndex<glm::vec4>(
                    asset, accessor,
                    [&](glm::vec4 weights, std::size_t index) { skeleton.weights[vertexStart + index] = weights; });
            }

            vertexStart += primitives[primitiveIndex].vertices.size();
            ++primitiveIndex;
        }
    }
}

AlphaMode fastgltfToAlphaMode(fastgltf::AlphaMode mode)
{
    switch (mode)
    {
    case fastgltf::AlphaMode::Opaque:
        return AlphaMode::Opaque;
    case fastgltf::AlphaMode::Mask:
        return AlphaMode::Mask;
    case fastgltf::AlphaMode::Blend:
        return AlphaMode::Blend;
    default:
        return AlphaMode::Opaque;
    }
}

} // namespace

fastgltf::Asset AssetLoader::loadAsset(const std::filesystem::path &path)
{
    static fastgltf::Parser parser;
    auto data = fastgltf::GltfDataBuffer::FromPath(path);

    if (data.error() != fastgltf::Error::None)
    {
        std::cerr << "FastGLTF error loading from " << path << std::endl
                  << fastgltfErrorToString(data.error()) << std::endl;
        throw std::runtime_error("Failed to load asset");
    }

    auto assetResult =
        parser.loadGltfJson(data.get(), path.parent_path(),
                            fastgltf::Options::GenerateMeshIndices | fastgltf::Options::LoadExternalBuffers);

    if (auto error = assetResult.error(); error != fastgltf::Error::None)
        std::cerr << "fastgltf error while loading: " << path << std::endl;

    return std::move(assetResult.get());
}

std::vector<Primitive> AssetLoader::loadMeshes(const fastgltf::Asset &asset)
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
            auto *colorIt = gltfPrimitive.findAttribute("COLOR_0");

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

            // Normals
            if (normalIt != gltfPrimitive.attributes.end())
            {
                auto &normalAccessor = asset.accessors[normalIt->accessorIndex];

                if (normalAccessor.type == fastgltf::AccessorType::Vec3)
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(
                        asset, normalAccessor, [&](glm::vec3 normal, std::size_t index) {
                            vertices[index].normal = glm::packSnorm3x10_1x2(glm::vec4{normal, 0.0f});
                            ;
                        });
                }
            }

            // Tangents
            if (tangentIt != gltfPrimitive.attributes.end())
            {
                auto &tangentAccessor = asset.accessors[tangentIt->accessorIndex];

                if (tangentAccessor.type == fastgltf::AccessorType::Vec4)
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(
                        asset, tangentAccessor, [&](glm::vec4 tangent, std::size_t index) {
                            vertices[index].tangent = glm::packSnorm3x10_1x2(tangent);
                        });
                }
            }

            // Texture coordinates
            if (texCoordIt != gltfPrimitive.attributes.end())
            {
                auto &texCoordAccessor = asset.accessors[texCoordIt->accessorIndex];

                if (texCoordAccessor.type == fastgltf::AccessorType::Vec2)
                {

                    fastgltf::iterateAccessorWithIndex<glm::vec2>(
                        asset, texCoordAccessor, [&](glm::vec2 texCoords, std::size_t index) {
                            vertices[index].texCoords = glm::packHalf2x16(texCoords);
                        });
                }
            }

            // Vertex color
            if (colorIt != gltfPrimitive.attributes.end())
            {
                auto &colorAccessor = asset.accessors[colorIt->accessorIndex];

                if (colorAccessor.type == fastgltf::AccessorType::Vec4)
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(
                        asset, colorAccessor,
                        [&](glm::vec4 color, std::size_t index) { vertices[index].color = glm::packUnorm4x8(color); });
                }

                else if (colorAccessor.type == fastgltf::AccessorType::Vec3)
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(
                        asset, colorAccessor, [&](glm::vec3 color, std::size_t index) {
                            vertices[index].color = glm::packUnorm4x8(glm::vec4{color.r, color.g, color.b, 1.0});
                        });
                }
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
                                              .materialIndex = materialIndex,
                                              .alphaMode = fastgltfToAlphaMode(alphaMode)});
        }
    }

    getNodeTransforms(asset, primitives);
    calculateAABB(primitives);

    return primitives;
}

std::vector<std::filesystem::path> AssetLoader::getURIs(const fastgltf::Asset &asset,
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
        else
        {
            std::cerr << "ERROR: Non-URI images not supported" << std::endl;
            throw std::runtime_error("Unsupported GLTF image format");
        }
    }

    return URIpaths;
}

AssetLoader::TextureDescriptor AssetLoader::getTextureInfo(const fastgltf::Asset &asset,
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

    return {sampler, assetImageIndex};
}

// Queues up pending textures for an asset, to be inserted into the texture array later
std::vector<AssetLoader::DeferredTexture> AssetLoader::texturesFromAsset(const fastgltf::Asset &asset,
                                                                         const std::filesystem::path &parentPath,
                                                                         Handle handle)
{
    using namespace AssetLoader;

    std::vector<DeferredTexture> deferredTextures;
    deferredTextures.reserve(asset.images.size());

    // Resize to match fastgltf materials array
    std::vector<Material> materials;
    materials.resize(asset.materials.size());

    // Get image paths image data
    const std::vector<std::filesystem::path> paths{getURIs(asset, parentPath)};

    // Map image index to pending texture index for deduplication
    std::unordered_map<std::size_t, std::size_t> imageIndices;

    // If the image index already exists, add the material index
    auto getOrCreateTextureID = [&](const fastgltf::TextureInfo &textureInfo, Material::Type type,
                                    std::size_t materialIndex) {
        TextureDescriptor descriptor{getTextureInfo(asset, textureInfo)};
        auto [pendingTextureIndex, inserted] = imageIndices.try_emplace(descriptor.imageIndex, 0);

        if (descriptor.sampler.wrapS != descriptor.sampler.wrapT)
        {
            std::cerr << "Sampler wrap mismatch";
            throw std::runtime_error("Texture property error");
        }

        if (inserted)
        {
            pendingTextureIndex->second = deferredTextures.size();
            const bool repeat{descriptor.sampler.wrapS == static_cast<uint16_t>(fastgltf::Wrap::Repeat)};

            deferredTextures.emplace_back(DeferredTexture{
                .handle = handle,
                .path = {paths[descriptor.imageIndex]},
                .textureRepeat = repeat,
                .materialIndices = {materialIndex},
                .type = type,
            });
        }
        else
        {
            DeferredTexture &deferred{deferredTextures[pendingTextureIndex->second]};

            deferred.materialIndices.push_back(materialIndex);

            if (type != deferred.type)
                std::cerr << "WARNING: gLTF file contains multiple PBR types pointing to the same image" << std::endl;
        }
    };

    for (size_t i{0}; i < asset.materials.size(); ++i)
    {
        const fastgltf::Material &assetMaterial = asset.materials[i];
        Material material;

        if (assetMaterial.pbrData.baseColorTexture.has_value())
        {
            const fastgltf::TextureInfo &albedoInfo{assetMaterial.pbrData.baseColorTexture.value()};
            getOrCreateTextureID(albedoInfo, Material::albedo, i);
        }

        if (assetMaterial.pbrData.metallicRoughnessTexture.has_value())
        {
            // ARM map never gets sRGB or anisotropic filtering
            const fastgltf::TextureInfo &armInfo = assetMaterial.pbrData.metallicRoughnessTexture.value();
            getOrCreateTextureID(armInfo, Material::ARM, i);
        }

        if (assetMaterial.normalTexture.has_value())
        {
            // Normal maps get no sRGB or aniso
            const fastgltf::TextureInfo &normalInfo = assetMaterial.normalTexture.value();
            getOrCreateTextureID(normalInfo, Material::normal, i);
        }
    }

    return deferredTextures;
}

std::optional<Skeleton> AssetLoader::loadSkeleton(const fastgltf::Asset &asset, std::vector<Primitive> &primitives)
{
    if (asset.skins.empty())
        return {};

    if (asset.skins.size() > 1)
        std::cerr << "WARNING: Multiple skins not supported" << std::endl;

    const auto &skin = asset.skins.front();
    std::size_t numJoints{skin.joints.size()};

    std::size_t numVertices{0};
    for (const Primitive &primitive : primitives)
        numVertices += primitive.vertices.size();

    Skeleton skeleton(numJoints, numVertices);

    loadSkin(asset, skin, skeleton);
    loadWeightsAndIDs(asset, primitives, skeleton);

    for (std::size_t i{0}; i < asset.nodes.size(); ++i)
        for (std::size_t childIndex : asset.nodes[i].children)
            skeleton.parents[childIndex] = static_cast<int>(i);

    return skeleton;
}

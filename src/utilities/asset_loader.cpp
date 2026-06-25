#define GLM_ENABLE_EXPERIMENTAL

#include "utilities/asset_loader.h"

#include <algorithm>
#include <cstdint>
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
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "models/aabb.h"
#include "models/animation.h"
#include "models/primitive.h"
#include "models/skeleton.h"
#include "renderer/textures.h"
#include "scene/handle.h"

namespace
{
struct PrimitiveRange
{
    std::size_t start{0};
    std::size_t count{0};
};

std::string fastgltfToErrorString(fastgltf::Error error)
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

AlphaMode fastgltfToAlphaMode(fastgltf::AlphaMode mode)
{
    switch (mode)
    {
    case fastgltf::AlphaMode::Opaque:
        return AlphaMode::opaque;
    case fastgltf::AlphaMode::Mask:
        return AlphaMode::mask;
    case fastgltf::AlphaMode::Blend:
        return AlphaMode::blend;
    default:
        return AlphaMode::opaque;
    }
}

AnimationPath fastgltfToAnimationPath(fastgltf::AnimationPath path)
{
    switch (path)
    {
    case fastgltf::AnimationPath::Rotation:
        return AnimationPath::rotation;
    case fastgltf::AnimationPath::Scale:
        return AnimationPath::scale;
    case fastgltf::AnimationPath::Translation:
        return AnimationPath::translation;
    default:
        std::cerr << "ERROR: Unsupported gLTF animation path" << std::endl;
        throw std::runtime_error("Unsupported gLTF animation path");
    }
}

AnimationInterpolation fastgltfToAnimationInterpolation(fastgltf::AnimationInterpolation interpolation)
{
    switch (interpolation)
    {
    case fastgltf::AnimationInterpolation::Linear:
        return AnimationInterpolation::linear;
    case fastgltf::AnimationInterpolation::Step:
        return AnimationInterpolation::step;
    default:
        std::cerr << "ERROR: Unsupported gLTF animation interpolation: " << static_cast<std::uint8_t>(interpolation)
                  << std::endl;
        throw std::runtime_error("Unsupported gLTF interpolation");
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

std::vector<PrimitiveRange> getPrimitiveRanges(const fastgltf::Asset &asset)
{
    std::vector<PrimitiveRange> ranges(asset.meshes.size());

    std::size_t offset{0};
    for (std::size_t meshIndex{0}; meshIndex < asset.meshes.size(); ++meshIndex)
    {
        std::size_t numPrimitives{asset.meshes[meshIndex].primitives.size()};
        ranges[meshIndex].start = offset;
        ranges[meshIndex].count = numPrimitives;

        offset += numPrimitives;
    }

    return ranges;
}

void getNodeTransforms(const fastgltf::Asset &asset, std::vector<Primitive> &primitives)
{
    std::vector<PrimitiveRange> meshToPrimitive{getPrimitiveRanges(asset)};

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
                        primitives[i].modelSpaceTransform = nodeTransform;
                }
            });
    }
}

void loadSkin(const fastgltf::Asset &asset, const fastgltf::Skin &srcSkin, Skeleton::Skin &dstSkin)
{
    // Copy for later recursive traversal
    dstSkin.jointNodes.assign(srcSkin.joints.begin(), srcSkin.joints.end());

    if (srcSkin.inverseBindMatrices.has_value())
    {
        auto &accessor = asset.accessors[*srcSkin.inverseBindMatrices];

        fastgltf::iterateAccessorWithIndex<glm::mat4>(
            asset, accessor, [&](glm::mat4 mat, std::size_t index) { dstSkin.invBindMatrices[index] = mat; });
    }
}

void loadTRS(const fastgltf::Asset &asset, Skeleton &skeleton)
{
    for (std::size_t i{0}; i < asset.nodes.size(); ++i)
    {
        const auto &node = asset.nodes[i];

        if (const auto *trs = std::get_if<fastgltf::TRS>(&node.transform))
        {
            auto uniformScale = [](const glm::vec3 &scale) {
                constexpr float epsilon{0.0001};
                return std::abs(scale.x - scale.y) < epsilon && std::abs(scale.y - scale.z) < epsilon;
            };

            const auto &t = trs->translation;
            glm::vec3 translation{t.x(), t.y(), t.z()};
            skeleton.restTranslations[i] = translation;

            const auto &r = trs->rotation;
            const glm::quat rotation{r.w(), r.x(), r.y(), r.z()};
            skeleton.restRotations[i] = rotation;

            // Only use uniform scale
            const auto &s = trs->scale;
            const glm::vec3 scale{s.x(), s.y(), s.z()};
            if (!uniformScale(scale))
                std::cerr << "WARNING: Non uniform scale unsupported" << std::endl;

            skeleton.restScales[i] = scale;
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

            skeleton.restTranslations[i] = translation;
            skeleton.restRotations[i] = rotation;
            skeleton.restScales[i] = scale;
        }
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
                  << fastgltfToErrorString(data.error()) << std::endl;
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
        numPrimitives += gltfMesh.primitives.size();

    primitives.reserve(numPrimitives);

    for (const auto &gltfMesh : asset.meshes)
    {
        for (const auto &gltfPrimitive : gltfMesh.primitives)
        {
            std::vector<Primitive::Vertex> vertices;
            std::vector<std::uint32_t> indices;
            std::vector<Primitive::VertexSkin> vertexSkins;

            auto *positionIt = gltfPrimitive.findAttribute("POSITION");
            auto *normalIt = gltfPrimitive.findAttribute("NORMAL");
            auto *tangentIt = gltfPrimitive.findAttribute("TANGENT");
            auto *texCoordIt = gltfPrimitive.findAttribute("TEXCOORD_0");
            auto *colorIt = gltfPrimitive.findAttribute("COLOR_0");
            auto *weightIt = gltfPrimitive.findAttribute("WEIGHTS_0");
            auto *jointIt = gltfPrimitive.findAttribute("JOINTS_0");

            if (positionIt == gltfPrimitive.attributes.end())
                throw std::runtime_error{"fastGLTF loading error: Model has no position accessor"};

            auto &positionAccessor = asset.accessors[positionIt->accessorIndex];
            vertices.resize(positionAccessor.count);

            if (positionAccessor.type == fastgltf::AccessorType::Vec3)
            {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(
                    asset, positionAccessor,
                    [&vertices](glm::vec3 position, std::size_t index) { vertices[index].position = position; });
            }

            // Normals
            if (normalIt != gltfPrimitive.attributes.end())
            {
                auto &normalAccessor = asset.accessors[normalIt->accessorIndex];

                if (normalAccessor.type == fastgltf::AccessorType::Vec3)
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(
                        asset, normalAccessor, [&vertices](glm::vec3 normal, std::size_t index) {
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
                        asset, tangentAccessor, [&vertices](glm::vec4 tangent, std::size_t index) {
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
                        asset, texCoordAccessor, [&vertices](glm::vec2 texCoords, std::size_t index) {
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
                    fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, colorAccessor,
                                                                  [&vertices](glm::vec4 color, std::size_t index) {
                                                                      vertices[index].color = glm::packUnorm4x8(color);
                                                                  });
                }

                else if (colorAccessor.type == fastgltf::AccessorType::Vec3)
                {
                    fastgltf::iterateAccessorWithIndex<glm::vec3>(
                        asset, colorAccessor, [&vertices](glm::vec3 color, std::size_t index) {
                            vertices[index].color = glm::packUnorm4x8(glm::vec4{color.r, color.g, color.b, 1.0});
                        });
                }
            }

            if (jointIt != gltfPrimitive.attributes.end())
            {
                auto &accessor = asset.accessors[jointIt->accessorIndex];
                vertexSkins.resize(accessor.count);

                fastgltf::iterateAccessorWithIndex<glm::uvec4>(
                    asset, accessor,
                    [&](glm::uvec4 jointIDs, std::size_t index) { vertexSkins[index].boneIDs = jointIDs; });
            }

            if (weightIt != gltfPrimitive.attributes.end())
            {
                auto &accessor = asset.accessors[weightIt->accessorIndex];
                vertexSkins.resize(accessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec4>(
                    asset, accessor, [&](glm::vec4 weight, std::size_t index) { vertexSkins[index].weights = weight; });
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
            primitives.emplace_back(Primitive{
                .vertices = std::move(vertices),
                .indices = std::move(indices),
                .vertexSkins = !vertexSkins.empty()
                                ? std::optional{std::vector<Primitive::VertexSkin>{std::move(vertexSkins)}}
                                : std::nullopt,
                .materialIndex = materialIndex,
                .alphaMode = fastgltfToAlphaMode(alphaMode),
            });
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

std::optional<Skeleton> AssetLoader::loadSkeleton(const fastgltf::Asset &asset)
{
    if (asset.animations.empty())
        return {};

    if (asset.skins.size() > 1)
        std::cerr << "WARNING: Multiple skins not supported" << std::endl;

    std::size_t numJoints{asset.skins.empty() ? std::size_t{0} : asset.skins.front().joints.size()};
    std::size_t numNodes{asset.nodes.size()};

    Skeleton skeleton{numJoints ? Skeleton{numNodes, numJoints} : Skeleton{numNodes}};

    if (asset.skins.size())
        loadSkin(asset, asset.skins.front(), skeleton.skin.value());

    loadTRS(asset, skeleton);

    for (std::size_t i{0}; i < asset.nodes.size(); ++i)
        for (std::size_t childIndex : asset.nodes[i].children)
            skeleton.parents[childIndex] = static_cast<int>(i);

    // Map nodes to primitives, so that animation channels can write back to a primitive's model space transform
    std::vector<PrimitiveRange> ranges{getPrimitiveRanges(asset)};

    for (std::size_t i{0}; i < asset.nodes.size(); ++i)
    {
        const auto meshIndex = asset.nodes[i].meshIndex;

        if (!meshIndex.has_value())
            continue;

        const PrimitiveRange &range{ranges[meshIndex.value()]};
        std::vector<size_t> &indices{skeleton.nodeToPrimitive[i]};

        indices.reserve(range.count);

        for (std::size_t j{range.start}; j < range.start + range.count; ++j)
            indices.push_back(j);
    }

    return skeleton;
}

std::optional<std::vector<Animation>> AssetLoader::loadAnimations(const fastgltf::Asset &asset)
{
    if (asset.animations.empty())
        return {};

    std::vector<Animation> animations(asset.animations.size());

    for (std::size_t i{0}; i < asset.animations.size(); ++i)
    {
        const fastgltf::Animation &animation{asset.animations[i]};
        animations[i].name = animation.name;

        animations[i].samplers.resize(animation.samplers.size());
        animations[i].channels.resize(animation.channels.size());

        // Samplers
        for (std::size_t j{0}; j < animation.samplers.size(); ++j)
        {
            const fastgltf::AnimationSampler &srcSampler{animation.samplers[j]};
            AnimationSampler &dstSampler{animations[i].samplers[j]};

            if (srcSampler.interpolation == fastgltf::AnimationInterpolation::CubicSpline)
            {
                std::cerr << "ERROR: Cubic spline interpolation not supported" << std::endl;
                throw std::runtime_error("Unsupported animation interpolation");
            }

            dstSampler.interpolation = fastgltfToAnimationInterpolation(srcSampler.interpolation);

            const auto &inputAccessor = asset.accessors[srcSampler.inputAccessor];
            dstSampler.times.resize(inputAccessor.count);

            fastgltf::iterateAccessorWithIndex<float>(
                asset, inputAccessor, [&dstSampler](float time, std::size_t index) { dstSampler.times[index] = time; });
        }

        // Channels
        for (std::size_t j{0}; j < animation.channels.size(); ++j)
        {
            const fastgltf::AnimationChannel srcChannel{animation.channels[j]};

            std::size_t srcAccessorIndex{animation.samplers[srcChannel.samplerIndex].outputAccessor};
            const auto &channelAccessor = asset.accessors[srcAccessorIndex];
            AnimationSampler &dstSampler{animations[i].samplers[srcChannel.samplerIndex]};

            dstSampler.values.resize(channelAccessor.count);

            switch (srcChannel.path)
            {
            case fastgltf::AnimationPath::Translation:
            case fastgltf::AnimationPath::Scale:
                fastgltf::iterateAccessorWithIndex<glm::vec3>(
                    asset, channelAccessor,
                    [&dstSampler](glm::vec3 rOrS, std::size_t index) { dstSampler.values[index] = rOrS; });

                break;

            case fastgltf::AnimationPath::Rotation:
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                    asset, channelAccessor, [&dstSampler](fastgltf::math::fvec4 vec, std::size_t index) {
                        dstSampler.values[index] = glm::quat{vec.w(), vec.x(), vec.y(), vec.z()};
                    });
                break;

            default:
                std::cerr << "WARNING: Unsupported gltf animation path" << std::endl;
            }

            AnimationChannel &dstChannel{animations[i].channels[j]};
            dstChannel.path = fastgltfToAnimationPath(srcChannel.path);
            dstChannel.samplerIndex = srcChannel.samplerIndex;

            if (!srcChannel.nodeIndex.has_value())
            {
                std::cerr << "ERROR: No node target for animation channel" << std::endl;
                throw std::runtime_error("Empty animation channel target node");
            }

            dstChannel.targetNode = srcChannel.nodeIndex.value();
        }

        // Find global start and end animation time
        for (const AnimationSampler &sampler : animations[i].samplers)
        {
            float &globalStart{animations[i].start};
            float &globalEnd{animations[i].end};
            float samplerStart{sampler.times.front()};
            float samplerEnd{sampler.times.back()};

            if (samplerStart < globalStart)
                globalStart = samplerStart;

            if (samplerEnd > globalEnd)
                globalEnd = samplerEnd;
        }
    }

    return animations;
}
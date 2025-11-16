#include "fastgltf_loader.h"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "primitive.h"

std::string fastgltfErrorToString(fastgltf::Error error) {
  switch (error) {
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

fastgltf::Asset fastgltfLoader::loadAsset(std::filesystem::path path) {
  static fastgltf::Parser parser;
  auto data = fastgltf::GltfDataBuffer::FromPath(path);

  if (data.error() != fastgltf::Error::None) {
    std::cerr << "FastGLTF error loading from " << path << std::endl;
    std::cerr << fastgltfErrorToString(data.error()) << std::endl;
    return fastgltf::Asset{};
  }

  auto assetResult =
      parser.loadGltfJson(data.get(), path.parent_path(),
                          fastgltf::Options::GenerateMeshIndices |
                              fastgltf::Options::LoadExternalBuffers);

  if (auto error = assetResult.error(); error != fastgltf::Error::None)
    std::cerr << "fastgltf error while loading: " << path << std::endl;

  return std::move(assetResult.get());
}

std::vector<Primitive::Mesh> fastgltfLoader::loadMeshes(
    const fastgltf::Asset& asset) {
  std::vector<Primitive::Mesh> meshes;

  for (const fastgltf::Mesh& gltfMesh : asset.meshes) {
    Primitive::Mesh mesh;
    mesh.primitives.reserve(gltfMesh.primitives.size());

    for (const fastgltf::Primitive& gltfPrimitive : gltfMesh.primitives) {
      std::vector<Primitive::Vertex> vertices;
      std::vector<std::uint32_t> indices;

      auto* positionIt = gltfPrimitive.findAttribute("POSITION");
      auto* normalIt = gltfPrimitive.findAttribute("NORMAL");
      auto* tangentIt = gltfPrimitive.findAttribute("TANGENT");
      auto* texCoordIt = gltfPrimitive.findAttribute("TEXCOORD_0");

      if (!positionIt)
        throw std::runtime_error{
            "fastGLTF loading error: Model has no position accessor"};

      auto& positionAccessor = asset.accessors[positionIt->accessorIndex];
      vertices.resize(positionAccessor.count);

      fastgltf::iterateAccessorWithIndex<glm::vec3>(
          asset, positionAccessor, [&](glm::vec3 position, std::size_t index) {
            vertices[index].position = position;
          });

      // Normals
      if (normalIt) {
        auto& normalAccessor = asset.accessors[normalIt->accessorIndex];

        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            asset, normalAccessor, [&](glm::vec3 normal, std::size_t index) {
              vertices[index].normal = normal;
            });
      }

      // Tangents
      if (tangentIt) {
        auto& tangentAccessor = asset.accessors[tangentIt->accessorIndex];

        fastgltf::iterateAccessorWithIndex<glm::vec4>(
            asset, tangentAccessor, [&](glm::vec4 tangent, std::size_t index) {
              vertices[index].tangent = tangent;
            });
      }

      // Texture coordinates
      if (texCoordIt) {
        auto& texCoordAccessor = asset.accessors[texCoordIt->accessorIndex];

        fastgltf::iterateAccessorWithIndex<glm::vec2>(
            asset, texCoordAccessor,
            [&](glm::vec2 texCoords, std::size_t index) {
              vertices[index].texCoords = texCoords;
            });
      }

      // Indices
      if (gltfPrimitive.indicesAccessor.has_value()) {
        auto& indicesAccessor =
            asset.accessors[gltfPrimitive.indicesAccessor.value()];
        indices.resize(indicesAccessor.count);

        std::size_t idx{0};
        fastgltf::iterateAccessor<std::uint32_t>(
            asset, indicesAccessor,
            [&](std::uint32_t index) { indices[idx++] = index; });
      }

      // Check if there are any materials at all
      unsigned int materialIndex = 0;
      if (gltfPrimitive.materialIndex.has_value())
        materialIndex =
            static_cast<unsigned int>(gltfPrimitive.materialIndex.value());

      // Build the primitive
      mesh.primitives.emplace_back(std::move(vertices), std::move(indices),
                                   materialIndex);
    }

    // Add the mesh
    meshes.push_back(std::move(mesh));
  }

  return meshes;
}

std::vector<std::filesystem::path> fastgltfLoader::getURIs(
    const fastgltf::Asset& asset, const std::filesystem::path parentPath) {
  std::vector<std::filesystem::path> URIpaths;
  URIpaths.reserve(asset.images.size());

  for (int i = 0; i < asset.images.size(); ++i) {
    fastgltf::Image image = asset.images[i];

    if (auto* uri = std::get_if<fastgltf::sources::URI>(&image.data)) {
      std::filesystem::path relativeImagePath(uri->uri.path().begin(),
                                              uri->uri.path().end());

      std::filesystem::path fullImagePath = parentPath / relativeImagePath;

      if (!uri->uri.isLocalPath())
        throw std::runtime_error{"URI not local: " + fullImagePath.string()};

      URIpaths.emplace_back(fullImagePath);
    }
  }

  return URIpaths;
}

std::pair<unsigned int, Sampler> fastgltfLoader::getTextureInfo(
    const fastgltf::Asset& asset, const fastgltf::TextureInfo& texInfo) {
  const auto& texture = asset.textures[texInfo.textureIndex];

  auto magFilter = fastgltf::Filter::Linear;
  auto minFilter = fastgltf::Filter::LinearMipMapLinear;
  auto wrapS = fastgltf::Wrap::Repeat;
  auto wrapT = fastgltf::Wrap::Repeat;

  if (texture.samplerIndex.has_value()) {
    auto samplerIndex = texture.samplerIndex.value();
    wrapS = asset.samplers[samplerIndex].wrapS;
    wrapT = asset.samplers[samplerIndex].wrapT;

    if (asset.samplers[samplerIndex].magFilter.has_value())
      magFilter = asset.samplers[samplerIndex].magFilter.value();

    if (asset.samplers[samplerIndex].minFilter.has_value())
      minFilter = asset.samplers[samplerIndex].minFilter.value();
  }

  const unsigned int assetImageIndex =
      static_cast<unsigned int>(texture.imageIndex.value());
  Sampler sampler{static_cast<uint16_t>(magFilter),
                  static_cast<uint16_t>(minFilter),
                  static_cast<uint16_t>(wrapS), static_cast<uint16_t>(wrapT)};

  return {assetImageIndex, sampler};
}
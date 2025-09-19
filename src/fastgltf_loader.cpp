#include "../headers/fastgltf_loader.h"
#include "../headers/primitive.h"
#include <iostream>
#include <vector>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>

fastgltf::Asset fastgltfLoadAsset(std::filesystem::path path) {

	static fastgltf::Parser parser;
	auto data = fastgltf::GltfDataBuffer::FromPath(path);

	if (data.error() != fastgltf::Error::None)
		std::cerr << "FastGLTF error loading from " << path << std::endl;

	//JSON only for now
	auto assetResult = parser.loadGltfJson(data.get(), path.parent_path(), fastgltf::Options::GenerateMeshIndices | fastgltf::Options::LoadExternalBuffers);

	if (auto error = assetResult.error(); error != fastgltf::Error::None)
		std::cerr << "FastGLTF buffer/parsing/validation error from " << path << std::endl;

	fastgltf::Asset asset = std::move(assetResult.get());
	return asset;
}

std::vector<Primitive::Mesh> fastgltfLoadMeshes(const fastgltf::Asset& asset) {
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

			auto& positionAccessor = asset.accessors[positionIt->accessorIndex];
			auto& normalAccessor = asset.accessors[normalIt->accessorIndex];
			auto& tangentAccessor = asset.accessors[tangentIt->accessorIndex];
			auto& texCoordAccessor = asset.accessors[texCoordIt->accessorIndex];
			auto& indicesAccessor = asset.accessors[gltfPrimitive.indicesAccessor.value()];

			if (!positionAccessor.bufferViewIndex.has_value()) {
				std::cerr << "No position data" << std::endl;
				continue;
			}

			vertices.resize(positionAccessor.count);
			indices.resize(indicesAccessor.count);

			fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, positionAccessor,
				[&](glm::vec3 position, std::size_t index) {
					vertices[index].position = position;
				});

			fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, normalAccessor,
				[&](glm::vec3 normal, std::size_t index) {
					vertices[index].normal = normal;
				});

			fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, tangentAccessor,
				[&](glm::vec4 tangent, std::size_t index) {
					vertices[index].tangent = tangent;
				});

			fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, texCoordAccessor,
				[&](glm::vec2 texCoords, std::size_t index) {
					vertices[index].texCoords = texCoords;
				});

			std::size_t idx = 0;
			fastgltf::iterateAccessor<std::uint32_t>(asset, indicesAccessor,
				[&](std::uint32_t index) {
					indices[idx++] = index;
				});

			mesh.primitives.emplace_back(std::move(vertices), std::move(indices), gltfPrimitive.materialIndex);
		}

		meshes.push_back(std::move(mesh));
	}
	return meshes;
}
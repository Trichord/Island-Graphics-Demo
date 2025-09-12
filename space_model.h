#pragma once

#include <vector>
#include "cube_mapped_model.h"
#include "glad/glad.h"
#include <glm/glm.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include "primitive.h"

class SpaceModel : public CubeMappedModel {
public:
    SpaceModel(std::filesystem::path path) {
		texturePaths = {
            "./Textures/nebula/right.png",
            "./Textures/nebula/left.png",
            "./Textures/nebula/top.png",
            "./Textures/nebula/bottom.png",
            "./Textures/nebula/front.png",
            "./Textures/nebula/back.png"
		};

		loadTextures();
		loadModel(path);
	}

	void draw() {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_CUBE_MAP, modelTexID);

		for (const Primitive::Mesh& mesh : meshes)
			for (const Primitive& primitive : mesh.primitives)
				primitive.draw();

		glBindVertexArray(0);
		glActiveTexture(GL_TEXTURE0);
	}

protected:
	std::vector<Primitive::Mesh> meshes;

	void loadModel(std::filesystem::path &path) {
		fastgltf::Parser parser;
		auto data = fastgltf::GltfDataBuffer::FromPath(path);

		if (data.error() != fastgltf::Error::None) {
			std::cerr << "FastGLTF error loading from " << path << std::endl;
			return;
		}

		//JSON only for now
		auto assetResult = parser.loadGltfJson(data.get(), path.parent_path(), fastgltf::Options::GenerateMeshIndices | fastgltf::Options::LoadExternalBuffers);

		if (auto error = assetResult.error(); error != fastgltf::Error::None) {
			std::cerr << "FastGLTF buffer/parsing/validation error from " << path << std::endl;
			return;
		}

		fastgltf::Asset& asset = assetResult.get();
		meshes.resize(asset.meshes.size());

		loadMeshes(asset);
	}

	void loadMeshes(const fastgltf::Asset& asset) {
		for (const fastgltf::Mesh& gltfMesh : asset.meshes) {
			Primitive::Mesh mesh;
			mesh.primitives.reserve(gltfMesh.primitives.size());

			for (const fastgltf::Primitive& gltfPrimitive : gltfMesh.primitives) {
				std::vector<Primitive::Vertex> vertices;
				std::vector<std::uint32_t> indices;

				auto* positionIt = gltfPrimitive.findAttribute("POSITION");
				auto* normalIt = gltfPrimitive.findAttribute("NORMAL");
				auto* tangentIt = gltfPrimitive.findAttribute("TANGENT");

				auto& positionAccessor = asset.accessors[positionIt->accessorIndex];
				auto& normalAccessor = asset.accessors[normalIt->accessorIndex];
				auto& tangentAccessor = asset.accessors[tangentIt->accessorIndex];
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

				std::size_t idx = 0;
				fastgltf::iterateAccessor<std::uint32_t>(asset, indicesAccessor,
					[&](std::uint32_t index) {
						indices[idx++] = index;
					});

				//Textures...
				mesh.primitives.emplace_back(std::move(vertices), std::move(indices));
			}
			meshes.push_back(std::move(mesh));
		}
	}
};
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
#include "fastgltf_loader.h"

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
		fastgltf::Asset asset = fastgltfLoadAsset(path);
		meshes.resize(asset.meshes.size());
		meshes = fastgltfLoadMeshes(asset);
	}
};
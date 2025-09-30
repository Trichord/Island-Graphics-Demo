#include "../headers/space_model.h"
#include "../headers/fastgltf_loader.h"
#include "../headers/primitive.h"
#include "../headers/shader.h"
#include <vector>
#include <filesystem>
#include "glad/glad.h"
#include <fastgltf/types.hpp>

SpaceModel::SpaceModel(std::filesystem::path path) {
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

void SpaceModel::render(Shader &shader) {
	shader.use();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTextureID);

	for (const Primitive::Mesh& mesh : meshes)
		for (const Primitive& primitive : mesh.primitives)
			primitive.draw();

	glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0);
}

void SpaceModel::loadModel(std::filesystem::path& path) {
	fastgltf::Asset asset = fastgltf_loader::loadAsset(path);
	meshes.resize(asset.meshes.size());
	meshes = fastgltf_loader::loadMeshes(asset);
}
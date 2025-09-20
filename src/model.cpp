#include "../headers/model.h"
#include "../headers/texture_manager.h"
#include "../headers/image_loader.h"
#include "../headers/fastgltf_loader.h"
#include "../headers/primitive.h"
#include "../headers/shader.h"
#include "../headers/thread_pool.h"
#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <glad/glad.h>
#include <fastgltf/types.hpp>
#include <fastgltf/glm_element_traits.hpp>

Model::Model(std::string path) {
	loadModel(path);
}

Model::Model(Model&& other) noexcept :
	asset(std::move(other.asset)),
	meshes(std::move(other.meshes)),
	materials(std::move(other.materials)),
	assetImages(std::move(other.assetImages)) {

	for (unsigned int i = 0; i < other.assetImages.size(); ++i)
		other.assetImages[i] = 0;
}

Model& Model::operator=(Model&& other) noexcept {

	if (this != &other) {

		for (const GLuint& textureID : assetImages)
			glDeleteTextures(1, &textureID);

		asset = std::move(other.asset);
		meshes = std::move(other.meshes);
		materials = std::move(other.materials);
		assetImages = std::move(other.assetImages);

		assetImages.resize(other.assetImages.size(), 0);
		other.asset = fastgltf::Asset{};

		for (unsigned int i = 0; i < other.assetImages.size(); ++i) {
			assetImages[i] = other.assetImages[i];
			other.assetImages[i] = 0;
		}
	}

	return *this;
}

Model::~Model() {
	for (const GLuint& textureID : assetImages)
		glDeleteTextures(1, &textureID);
}

void Model::draw(Shader& shader) {
	shader.use();

	for (const Primitive::Mesh& mesh : meshes) {
		for (const Primitive& primitive : mesh.primitives) {
			Material material = materials[primitive.materialIndex];

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, assetImages[material.albedoImageIndex]);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, assetImages[material.metallicRoughnessImageIndex]);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, assetImages[material.normalImageIndex]);

			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, assetImages[material.aoImageIndex]);

			primitive.draw();
		}
	}

	glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0);
}

void Model::loadModel(std::filesystem::path path) {
	asset = fastgltf_loader::loadAsset(path);

	meshes.resize(asset.meshes.size());
	meshes = fastgltf_loader::loadMeshes(asset);

	assetImages.resize(asset.images.size());
	loadTextures(path.parent_path());

	asset = fastgltf::Asset{};
}

void Model::loadTextures(const std::filesystem::path parentPath) {
	//Get all local Uniform Resource Identifiers from gltf, so we can pass the file paths to the image loader
	std::vector<std::filesystem::path> texturePaths = fastgltf_loader::getURIs(asset, parentPath);

	//Load images
	ImageLoader imageLoader(texturePaths);
	std::vector<ImageData> imageData = imageLoader.getImageData();

	//Pass the image data so that OpenGL texture IDs can be generated and attached to the appropriate materials
	loadMaterials(imageData);
}

void Model::loadMaterials(const std::vector<ImageData>& imageData) {
	materials.resize(asset.materials.size());

	auto processMaterialTexture = [&](const fastgltf::TextureInfo& textureInfo, bool sRGB) {
		auto [index, sampler] = fastgltf_loader::getTextureInfo(asset, textureInfo);

		//Generate new texture id if the index is not already in the set image indices already processed
		if (processedImageIndices.insert(index).second) {
			GLuint texID = texture_manager::generateTextureID(imageData[index], sampler, index, sRGB);
			assetImages[index] = texID;
			processedImageIndices.insert(index);
		}

		return static_cast<unsigned int>(index);
		};

	//Process all materials in the gltf asset
	for (unsigned int i = 0; i < asset.materials.size(); ++i) {
		const auto& material = asset.materials[i];

		auto& baseColorTexture = material.pbrData.baseColorTexture.value();
		materials[i].albedoImageIndex = processMaterialTexture(baseColorTexture, true);

		auto& metallicRoughnessTexture = material.pbrData.metallicRoughnessTexture.value();
		materials[i].metallicRoughnessImageIndex = processMaterialTexture(metallicRoughnessTexture, false);

		auto& normalTexture = material.normalTexture.value();
		materials[i].normalImageIndex = processMaterialTexture(normalTexture, false);

		auto& occlusionTexture = material.occlusionTexture.value();
		materials[i].aoImageIndex = processMaterialTexture(occlusionTexture, false);
	}
}
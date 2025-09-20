#include "../headers/model.h"
#include "../headers/texture.h"
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
#include <glm/glm.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
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
		other.asset = fastgltf::Asset {};

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
	asset = fastgltfLoadAsset(path);
	
	meshes.resize(asset.meshes.size());
	meshes = fastgltfLoadMeshes(asset);

	assetImages.resize(asset.images.size());
	loadTextures(path.parent_path());

	asset = fastgltf::Asset{};
}

void Model::loadTextures(const std::filesystem::path parentPath) {
	std::vector<std::filesystem::path> texturePaths = getURIs(parentPath);
	ImageLoader imageLoader(texturePaths);
	std::vector<ImageData> imageData = imageLoader.getImageData();

	getMaterials(imageData);

	//generateTextureIDs(imageData);
}

std::vector<std::filesystem::path> Model::getURIs(const std::filesystem::path parentPath) {
	std::vector<std::filesystem::path> URIpaths;
	URIpaths.reserve(asset.images.size());

	for (unsigned int i = 0; i < asset.images.size(); ++i) {
		fastgltf::Image image = asset.images[i];

		if (auto* uri = std::get_if<fastgltf::sources::URI>(&image.data)) {
			std::filesystem::path relativeImagePath(uri->uri.path().begin(), uri->uri.path().end());
			std::filesystem::path fullImagePath = parentPath / relativeImagePath;

			if (!uri->uri.isLocalPath()) 
				throw std::runtime_error("URI not local: " + fullImagePath.string());
		
			URIpaths.emplace_back(fullImagePath);
		}	
	}

	return URIpaths;
}

void Model::getMaterials(const std::vector<ImageData>& imageData) {
	materials.resize(asset.materials.size());
	
	for (unsigned int i = 0; i < asset.materials.size(); ++i){
		const auto &material = asset.materials[i];
		
		auto& baseColorTexture = material.pbrData.baseColorTexture.value();
		auto [baseTexImageIndex, baseTexSampler] = getTextureInfo(baseColorTexture);
		materials[i].albedoImageIndex = static_cast<unsigned int>(baseTexImageIndex);
		generateTextureID(imageData[baseTexImageIndex], baseTexSampler, baseTexImageIndex, true);
		
		auto& metallicRoughnessTexture = material.pbrData.metallicRoughnessTexture.value();
		auto [metallicTexImageIndex, metallicSampler] = getTextureInfo(metallicRoughnessTexture);
		materials[i].metallicRoughnessImageIndex = static_cast<unsigned int>(metallicTexImageIndex);
		generateTextureID(imageData[metallicTexImageIndex], metallicSampler, metallicTexImageIndex, false);
		
		auto& normalTexture = material.normalTexture.value();
		auto [normalTexImageIndex, normalSampler] = getTextureInfo(normalTexture);
		materials[i].normalImageIndex = static_cast<unsigned int>(normalTexImageIndex);
		generateTextureID(imageData[normalTexImageIndex], normalSampler, normalTexImageIndex, false);

		auto& occlusionTexture = material.occlusionTexture.value();
		auto [aoImageIndex, aoSampler] = getTextureInfo(occlusionTexture);
		materials[i].aoImageIndex = static_cast<unsigned int>(aoImageIndex);
		generateTextureID(imageData[aoImageIndex], aoSampler, aoImageIndex, false);
	}
}

std::pair<unsigned int, Model::Sampler> Model::getTextureInfo(const fastgltf::TextureInfo& texInfo) const {
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

	const unsigned int assetImageIndex = static_cast<unsigned int>(texture.imageIndex.value());
	Sampler sampler = { static_cast<uint16_t>(magFilter), static_cast<uint16_t>(minFilter), static_cast<uint16_t>(wrapS), static_cast<uint16_t>(wrapT) };

	return { assetImageIndex, sampler };
}

void Model::generateTextureID(const ImageData& image, Sampler sampler, unsigned int index, bool sRGB) {
	if (processedImageIndices.find(index) != processedImageIndices.end())
		return;

	GLuint textureID;

	if (image.data) {
		GLenum imageFormat = 3, internalFormat = 3;
		int nrChannels = image.nrChannels;

		//stb_image only ever returns a 1, 2, 3, or 4 for a valid image
		if (nrChannels == 1) {
			imageFormat = GL_RED;
			internalFormat = GL_RED;
		}
		else if (nrChannels == 2) {
			imageFormat = GL_RG;
			internalFormat = GL_RG;
		}
		else if (nrChannels == 3) {
			imageFormat = GL_RGB;
			internalFormat = sRGB ? GL_SRGB : GL_RGB;
		}
		else if (nrChannels == 4) {
			imageFormat = GL_RGBA;
			internalFormat = sRGB ? GL_SRGB_ALPHA : GL_RGBA;
		}

		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);

		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, image.width, image.height, 0, imageFormat, GL_UNSIGNED_BYTE, image.data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampler.wrapS);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampler.wrapT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampler.minFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampler.magFilter);
	}
	else
		textureID = 0;

	assetImages[index] = textureID;
	glActiveTexture(GL_TEXTURE0);

	processedImageIndices.insert(index);
}
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

		meshes = std::move(other.meshes);
		materials = std::move(other.materials),
		assetImages = std::move(other.assetImages);
		
		assetImages.resize(other.assetImages.size(), 0);

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

	//textureIDs contains, in this order: albedo, metallic-roughness, normal, occlusion
	for (unsigned int i = 0; i < 4; ++i) { //This loop will not work with multiple materials
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, assetImages[i]);
	}

	for (const Primitive::Mesh& mesh : meshes)
		for (const Primitive& primitive : mesh.primitives)
			primitive.draw();

	glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0);
}

void Model::loadModel(std::filesystem::path path) {
	fastgltf::Asset asset = fastgltfLoadAsset(path);
	
	meshes.resize(asset.meshes.size());
	meshes = fastgltfLoadMeshes(asset);

	loadMaterials(asset);

	loadTextures(asset, path.parent_path());
}

void Model::loadTextures(const fastgltf::Asset& asset, const std::filesystem::path parentPath) {
	std::vector<std::filesystem::path> texturePaths = getURIs(asset, parentPath);
	ImageLoader imageLoader(texturePaths);
	std::vector<ImageData> imageData = imageLoader.getImageData();



	generateTextureIDs(imageData);
}

std::vector<std::filesystem::path> Model::getURIs(const fastgltf::Asset& asset, const std::filesystem::path parentPath) {
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

void Model::generateTextureID(const ImageData &image, bool sRGB) {
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

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else 
			textureID = 0;
		
		assetImages[i] = textureID;
	}

	glActiveTexture(GL_TEXTURE0);
}

void Model::loadMaterials(const fastgltf::Asset& asset) {
	materials.resize(asset.materials.size());
	
	for (const fastgltf::Material& material : asset.materials) {

		auto getTexInfo = [&](const fastgltf::TextureInfo& texInfo) {
			const auto& texture = asset.textures[texInfo.textureIndex];
			const unsigned int assetImageIndex = texture.imageIndex.value();
		
			auto wrapS = fastgltf::Wrap::Repeat;
			auto wrapT = fastgltf::Wrap::Repeat;

			if (texture.samplerIndex.has_value()) {
				wrapS = asset.samplers[texture.samplerIndex.value()].wrapS;
				wrapT = asset.samplers[texture.samplerIndex.value()].wrapT;
			}


			
		};

		if (material.pbrData.baseColorTexture.has_value()) {
			getTexInfo(material.pbrData.baseColorTexture.value());
			
		}
		
		
		getTexInfo(material.pbrData.metallicRoughnessTexture.value()); //TEXTURE_METALLIC_ROUGHNESS
		getTexInfo(material.normalTexture.value()); //TEXTURE_NORMAL
		getTexInfo(material.occlusionTexture.value()); //TEXTURE_OCCLUSION
	}
	
}
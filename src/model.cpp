#include "../headers/model.h"
#include "../headers/texture.h"
#include "../headers/image_loader.h"
#include "../headers/fastgltf_loader.h"
#include "../headers/primitive.h"
#include "../headers/shader.h"
#include "../headers/thread_pool.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>

Model::Model(std::string path) {
	textureIDs.fill(0);
	loadModel(path);
}

Model::Model(Model&& other) noexcept :
	meshes(std::move(other.meshes)),
	texturePaths(std::move(other.texturePaths)),
	textureIDs(std::move(other.textureIDs)) {
	other.texturePaths.clear();
	other.textureIDs.fill(0);
}

Model& Model::operator=(Model&& other) noexcept {

	if (this != &other) {

		for (const GLuint& textureID : textureIDs)
			glDeleteTextures(1, &textureID);

		meshes = std::move(other.meshes);
		texturePaths = std::move(other.texturePaths);
		textureIDs = std::move(other.textureIDs);
		other.texturePaths.clear();
		other.textureIDs.fill(0);
	}

	return *this;
}

Model::~Model() {
	for (const GLuint& textureID : textureIDs)
		glDeleteTextures(1, &textureID);
}

void Model::draw(Shader& shader) {
	shader.use();

	//textureIDs contains, in this order: albedo, metallic-roughness, normal, occlusion
	for (unsigned int i = 0; i < 4; ++i) {
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, textureIDs[i]);
	}

	for (const Primitive::Mesh& mesh : meshes)
		for (const Primitive& primitive : mesh.primitives)
			primitive.draw();

	glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0);
}

void Model::loadModel(std::filesystem::path path) {
	fastgltf::Asset asset = fastgltfLoadAsset(path);
	preloadTextures(asset, path.parent_path());
	meshes.resize(asset.meshes.size());
	meshes = fastgltfLoadMeshes(asset);
}

//Fill "textureMap" map with all scene textures
void Model::preloadTextures(const fastgltf::Asset& asset, const std::filesystem::path parentPath) {
	for (const fastgltf::Material& material : asset.materials) {

		auto getTexInfo = [&](const fastgltf::TextureInfo& texInfo, const TextureType& texType) {
			const auto& texture = asset.textures[texInfo.textureIndex];
			const auto& image = asset.images[texture.imageIndex.value()];

			/* Texture wrap options
			auto wrapS = fastgltf::Wrap::Repeat;
			auto wrapT = fastgltf::Wrap::Repeat;

			if (texture.samplerIndex.has_value()) {
				wrapS = asset.samplers[texture.samplerIndex.value()].wrapS;
				wrapT = asset.samplers[texture.samplerIndex.value()].wrapT;
			}
			*/

			if (auto* uri = std::get_if<fastgltf::sources::URI>(&image.data)) {
				std::filesystem::path relativeImagePath(uri->uri.path().begin(), uri->uri.path().end());
				std::filesystem::path fullImagePath = parentPath / relativeImagePath;

				if (!uri->uri.isLocalPath()) {
					std::cerr << "URI not local: " << fullImagePath.string() << std::endl;
					return;
				}

				texturePaths.emplace(fullImagePath, texType);
			}
			};

		getTexInfo(material.pbrData.baseColorTexture.value(), TEXTURE_BASE_COLOR);
		getTexInfo(material.pbrData.metallicRoughnessTexture.value(), TEXTURE_METALLIC_ROUGHNESS);
		getTexInfo(material.normalTexture.value(), TEXTURE_NORMAL);
		getTexInfo(material.occlusionTexture.value(), TEXTURE_OCCLUSION);
	}
	generateTextureIDs();
}

//Retrieves image data from the ImageLoader (multi-threaded) and generates OpenGL Texture ID's
void Model::generateTextureIDs() {
	ImageLoader imageLoader(texturePaths);
	std::unordered_map<std::filesystem::path, ImageData> loadedImages = imageLoader.getImageData();

	//Generate new ID's
	for (auto const& [path, image] : loadedImages) {
		GLuint textureID;
		int nrChannels = image.nrChannels;

		if (image.data) {
			GLenum imageFormat = 3, internalFormat = 3;

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
				internalFormat = image.textureType == TEXTURE_BASE_COLOR ? GL_SRGB : GL_RGB;
			}
			else if (nrChannels == 4) {
				imageFormat = GL_RGBA;
				internalFormat = image.textureType == TEXTURE_BASE_COLOR ? GL_SRGB_ALPHA : GL_RGBA;
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
		else {
			textureID = 0;
			std::cerr << "STBI Failed to load image at: " << path.string() << std::endl;
		}
		assembleTexureInfo(image.textureType, textureID);
	}

	glActiveTexture(GL_TEXTURE0);
}

void Model::assembleTexureInfo(TextureType type, GLuint id) {
	if (type == TEXTURE_BASE_COLOR)
		textureIDs[0] = id;

	else if (type == TEXTURE_METALLIC_ROUGHNESS)
		textureIDs[1] = id;

	else if (type == TEXTURE_NORMAL)
		textureIDs[2] = id;

	else if (type == TEXTURE_OCCLUSION)
		textureIDs[3] = id;
}


#pragma once

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
#include "texture.h"
#include "shader.h"
#include "image_loader.h"
#include "primitive.h"
#include "thread_pool.h"

//Model owns the OpenGL Texture IDs and copying is disabled to avoid double frees.  Texture IDs are released on Model destruction
class Model {
public:
	struct Mesh {
		std::vector<Primitive> primitives;
	};

	Model(std::string path) {
		textureIDs.fill(0);
		loadModel(path);
	}

	Model(Model &&other) noexcept :
		meshes(std::move(other.meshes)),
		texturePaths(std::move(other.texturePaths)),
		textureIDs(std::move(other.textureIDs))	{
		other.texturePaths.clear();
		other.textureIDs.fill(0);
	}

	Model &operator=(Model &&other) noexcept {
		if (this != &other) {

			for (const GLuint &textureID : textureIDs)
				glDeleteTextures(1, &textureID);

			meshes = std::move(other.meshes);
			texturePaths = std::move(other.texturePaths);
			textureIDs = std::move(other.textureIDs);
			other.texturePaths.clear();
			other.textureIDs.fill(0);
		}

		return *this;
	}

	Model(const Model&) = delete;
	Model& operator=(Model const&) = delete;

	~Model() {
		for (const GLuint& textureID : textureIDs)
			glDeleteTextures(1, &textureID);
	}

	void draw(Shader &shader) {
		shader.use();

		//Base color, metallic-roughness, normal, occlusion
		for(unsigned int i = 0; i < 4; ++i){
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, textureIDs[i]);
		}

		for (const Mesh& mesh : meshes)
			for (const Primitive& primitive : mesh.primitives)
				primitive.draw();

		glBindVertexArray(0);
		glActiveTexture(GL_TEXTURE0);
	}

private:
	inline static fastgltf::Parser parser;
	std::vector<Mesh> meshes;
	std::unordered_map<std::string, TextureType> texturePaths;
	std::array<GLuint, 4> textureIDs;

	void loadModel(std::filesystem::path path) {
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

		preloadTextures(asset, path.parent_path());
		meshes.resize(asset.meshes.size());
		loadMeshes(asset);
	}

	void loadMeshes(const fastgltf::Asset& asset) {
		for (const fastgltf::Mesh& gltfMesh : asset.meshes) {
			Mesh mesh;
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

				//Textures...
				mesh.primitives.emplace_back(std::move(vertices), std::move(indices));
			}
			meshes.push_back(std::move(mesh));
		}
	}

	//Fill "textureMap" map with all scene textures
	void preloadTextures(const fastgltf::Asset &asset, const std::filesystem::path parentPath) {

		for (const fastgltf::Material& material : asset.materials) {

			auto getTexInfo = [&](const fastgltf::TextureInfo &texInfo, const TextureType &texType) {
				const auto& texture = asset.textures[texInfo.textureIndex];
				const auto& image = asset.images[texture.imageIndex.value()];

				if (auto* uri = std::get_if<fastgltf::sources::URI>(&image.data)) {
					std::string imagePath(uri->uri.path().begin(), uri->uri.path().end());
					imagePath = parentPath.string() + '/' + imagePath;

					if (!uri->uri.isLocalPath()) {
						std::cerr << "URI not local: " << imagePath << std::endl;
						return;
					}

					texturePaths.emplace(imagePath, texType);
				}
			};

			if (material.pbrData.baseColorTexture.has_value()) {
				const auto& texInfo = material.pbrData.baseColorTexture.value();
				
			}

			getTexInfo(material.pbrData.baseColorTexture.value(), TEXTURE_BASE_COLOR);
			getTexInfo(material.pbrData.metallicRoughnessTexture.value(), TEXTURE_METALLIC_ROUGHNESS);
			getTexInfo(material.normalTexture.value(), TEXTURE_NORMAL);
			getTexInfo(material.occlusionTexture.value(), TEXTURE_OCCLUSION);		
		}
		generateTextureIDs();
	}

	//Retrieves image data from the ImageLoader (multi-threaded) and generates OpenGL Texture ID's
	void generateTextureIDs() {
		ImageLoader imageLoader(texturePaths);
		std::unordered_map<std::string, ImageData> loadedImages = imageLoader.getImageData();

		//Generate new ID's
		for (auto const &[path, imageData] : loadedImages) {
			GLuint textureID;
			int nrChannels = imageData.nrChannels;
			unsigned char *data = imageData.data;
	
			if (data) {
				GLenum imageFormat;
				GLenum internalFormat;

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
					internalFormat = imageData.textureType == TEXTURE_BASE_COLOR ? GL_SRGB : GL_RGB;
				}
				else if (nrChannels == 4) {
					imageFormat = GL_RGBA;
					internalFormat = imageData.textureType == TEXTURE_BASE_COLOR ? GL_SRGB_ALPHA : GL_RGBA;
				}

				glGenTextures(1, &textureID);
				glBindTexture(GL_TEXTURE_2D, textureID);

				glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, imageData.width, imageData.height, 0, imageFormat, GL_UNSIGNED_BYTE, data);
				glGenerateMipmap(GL_TEXTURE_2D);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);	
			}
			else {
				textureID = 0;
				std::cerr << "STBI Failed to load image from: " << path << std::endl;
			}
			assembleTexureInfo(imageData.textureType, textureID);
		}

		glActiveTexture(GL_TEXTURE0);
	}

	void assembleTexureInfo(TextureType type, GLuint id){
		if (type == TEXTURE_BASE_COLOR)
			textureIDs[0] = id;

		else if (type == TEXTURE_METALLIC_ROUGHNESS)
			textureIDs[1] = id;

		else if (type == TEXTURE_NORMAL)
			textureIDs[2] = id;

		else if (type == TEXTURE_OCCLUSION)
			textureIDs[3] = id;
		
		else 
				std::cerr << "MODEL::MISSING TEXTURE TYPE" << std::endl;
	}
};


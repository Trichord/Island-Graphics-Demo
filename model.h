#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <filesystem>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include "shader.h"
#include "primitive.h"
#include "thread_pool.h"
#include "image_loader.h"

//Model owns the OpenGL Texture IDs and copying is disabled to avoid double frees.  Texture IDs are released on Model destruction
class Model {
public:
	struct Mesh {
		std::vector<Primitive> primitives;
	};

	Model(std::string path) {
		loadModel(path);
	}

	Model(Model &&other) noexcept :
		meshes(std::move(other.meshes)),
		loadedTextures(std::move(other.loadedTextures)) {
		other.loadedTextures.clear();
	}

	Model &operator=(Model &&other) noexcept {
		if (this != &other) {

			for (const auto &pair : loadedTextures)
				glDeleteTextures(1, &pair.second.id);

			meshes = std::move(other.meshes);
			loadedTextures = std::move(other.loadedTextures);

			other.loadedTextures.clear();
		}

		return *this;
	}

	Model(const Model&) = delete;
	Model& operator=(Model const&) = delete;

	~Model() {
		for (const auto &pair : loadedTextures)
			glDeleteTextures(1, &pair.second.id);
	}

	void draw(Shader &shader) {
		for (unsigned int i = 0; i < meshes.size(); i++)
			meshes[i].draw(shader);

		glBindVertexArray(0);
		glActiveTexture(GL_TEXTURE0);
	}

private:
	static fastgltf::Parser parser;
	std::vector<Mesh> meshes;
	std::unordered_map<std::string, Primitive::Texture> loadedTextures;

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

		meshes.resize(asset.meshes.size());

		for (fastgltf::Mesh &gltfMesh : asset.meshes) {
			Mesh mesh;
			mesh.primitives.reserve(gltfMesh.primitives.size());

			for (fastgltf::Primitive& gltfPrimitive : gltfMesh.primitives) {
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
				mesh.primitives.emplace_back(std::move(vertices), std::move(indices), );

			}
			
			
			
			
			
			
			if (auto* uri = std::get_if<fastgltf::sources::URI>(&image.data)) {
				
				if (!uri->uri.isLocalPath())
					continue;

				std::string imagePath(uri->uri.path().begin(), uri->uri.path().end());

				loadedTextures.emplace(imagePath, Texture());
			}
		}


		preloadTextures(scene);
		processNode(scene->mRootNode, scene);
	}

	//Fill "loadedTextures" map with all scene textures
	void preloadTextures(const aiScene *scene) {
		unsigned int totalTextures = 0;

		//Count
		for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
			aiMaterial *material = scene->mMaterials[i];
			totalTextures += material->GetTextureCount(aiTextureType_DIFFUSE);
			totalTextures += material->GetTextureCount(aiTextureType_SPECULAR);
		}

		loadedTextures.reserve(totalTextures);

		//Fill in loadedTextures map, minus the texture ID
		for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
			aiMaterial *material = scene->mMaterials[i];
			getTextureNames(material, aiTextureType_DIFFUSE, TEXTURE_DIFFUSE);
			getTextureNames(material, aiTextureType_SPECULAR, TEXTURE_SPECULAR);
		}

		//Texture ID's
		generateIDs();
	}

	//Build meshes for current node, then recurse on node's children
	void processNode(aiNode *node, const aiScene *scene) {
		for (unsigned int i = 0; i < node->mNumMeshes; i++) {
			aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
			meshes.emplace_back(processMesh(mesh, scene));
		}

		for (unsigned int i = 0; i < node->mNumChildren; i++)
			processNode(node->mChildren[i], scene);
	}

	//Turn assimp data into our vertices, indices, and textures	
	Mesh processMesh(aiMesh *mesh, const aiScene *scene) {
		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;
		std::vector<Texture> textures;

		vertices.reserve(mesh->mNumVertices);
		
		for (unsigned int i = 0; i < mesh->mNumVertices; i++) {		
			vertices.emplace_back(
				mesh->mVertices[i].x,
				mesh->mVertices[i].y,
				mesh->mVertices[i].z,

				mesh->mNormals[i].x,
				mesh->mNormals[i].y,
				mesh->mNormals[i].z,

				mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][i].x : 0,
				mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][i].y : 0
			);
		}
	
		//Count and reserve
		unsigned int totalIndices = 0;
		for (unsigned int i = 0; i < mesh->mNumFaces; i++)
			totalIndices += mesh->mFaces[i].mNumIndices;
		indices.reserve(totalIndices);

		//Build all indices
		for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
			aiFace face = mesh->mFaces[i];
			for (unsigned int j = 0; j < face.mNumIndices; j++)
				indices.emplace_back(face.mIndices[j]);
		}
		
		//Assign mesh-specific textures from loadedTextures map
		if (mesh->mMaterialIndex >= 0) {
			aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];

			//Get all the mesh's texture data from the preloaded list
			std::vector<Texture> diffuseMaps = texturesFromPreloaded(material, aiTextureType_DIFFUSE, TEXTURE_DIFFUSE);
			std::vector<Texture> specularMaps = texturesFromPreloaded(material, aiTextureType_SPECULAR, TEXTURE_SPECULAR);
			
			textures.reserve(diffuseMaps.size() + specularMaps.size());
			textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
			textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
		}

		return Mesh(std::move(vertices), std::move(indices), std::move(textures));
	}

	//Create filepath : Texture pair, if not already in loadedTextures map
	void getTextureNames(aiMaterial *mat, aiTextureType type,TextureType typeName) {
		for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
			aiString str;
			mat->GetTexture(type, i, &str);
			std::string filePath = str.C_Str();
			
			auto iterator = loadedTextures.find(filePath);

			if (iterator == loadedTextures.end())
				loadedTextures.emplace(filePath, Texture{ 0, typeName });
		}
	}
	
	//Looks through preloaded textures and builds a vector of textures for the given material
	std::vector<Texture> texturesFromPreloaded(aiMaterial *material, aiTextureType type, TextureType typeName) {
		std::vector<Texture> textures;
		textures.reserve(material->GetTextureCount(type));

		for (unsigned int i = 0; i < material->GetTextureCount(type); i++) {
			aiString str;
			material->GetTexture(type, i, &str);
			std::string name = str.C_Str();

			auto iterator = loadedTextures.find(name);

			if (iterator != loadedTextures.end())
				textures.emplace_back(iterator->second.id, iterator->second.type);
			else
				std::cout << "ERROR: Texture not pre-loaded!  Texture name: " << name << std::endl;
		}

		return textures;
	}

	void exportScene(const aiScene *scene, std::string &path) {
		Assimp::Exporter exporter;
		std::string directory = path.substr(0, path.find_last_of('/') + 1);
		std::string fileName = path.substr(path.find_last_of('/') + 1);
		fileName = fileName.substr(0, fileName.find_last_of('.'));
			
		exporter.Export(scene, "assbin", directory + fileName + ".assbin");
		std::cout << "Exported non .assbin file as \"" << fileName << ".assbin\" in: " << directory << std::endl;
	}

	//Retrieves image data from the ImageLoader (multi-threaded) and generates OpenGL Texture ID's
	void generateIDs() {
		ImageLoader imageLoader(loadedTextures);
		std::unordered_map<std::string, ImageData> loadedImages = imageLoader.getImageData();

		//Generate new ID's
		for (auto &imagePair : loadedImages) {
			unsigned int textureID;
			int nrChannels = imagePair.second.nrChannels;
			unsigned char *data = imagePair.second.data;
	
			if (data) {
				GLenum imageFormat;

				//stb_image only ever returns a 1, 2, 3, or 4 for a valid image
				if (nrChannels == 1)
					imageFormat = GL_RED;
				else if (nrChannels == 2)
					imageFormat = GL_RG;
				else if (nrChannels == 3)
					imageFormat = GL_RGB;
				else if (nrChannels == 4)
					imageFormat = GL_RGBA;

				glGenTextures(1, &textureID);
				glBindTexture(GL_TEXTURE_2D, textureID);

				glTexImage2D(GL_TEXTURE_2D, 0, imageFormat, imagePair.second.width, imagePair.second.height, 0, imageFormat, GL_UNSIGNED_BYTE, data);
				glGenerateMipmap(GL_TEXTURE_2D);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);	
			}
			else 
				textureID = 0;

			auto loadedTexIter = loadedTextures.find(imagePair.first);

			if (loadedTexIter != loadedTextures.end())
				loadedTexIter->second.id = textureID;
			else
				std::cout << "ERROR: Texture filepath not found in loadedTextures" << std::endl;
		}

		glActiveTexture(GL_TEXTURE0);
	}
};


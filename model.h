#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "shader.h"
#include "mesh.h"
#include "thread_pool.h"
#include "image_loader.h"

//Model owns the OpenGL Texture IDs and copying is disabled to avoid double frees.  Texture IDs are released on Model destruction
class Model {
public:
	Model(const Model &) = delete;
	Model &operator=(Model const &) = delete;

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
	std::vector<Mesh> meshes;
	std::unordered_map<std::string, Texture> loadedTextures;

	void loadModel(std::string path) {
		Assimp::Importer importer;
		const aiScene *scene;
		std::string fileFormat = path.substr(path.find_last_of('.'));

		if (fileFormat == ".assbin")
			scene = importer.ReadFile(path, 0);
		else
			scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			std::cout << "ERROR:ASSIMP::" << importer.GetErrorString() << std::endl;
			return;
		}

		//Export it for next time
		if (fileFormat != ".assbin")
			exportScene(scene, path);

		meshes.reserve(scene->mNumMeshes);

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

		//Save previous texture unit, switch to unit 0, and save currently bound texture
		GLint previousTexUnit = 0, previousTex = 0, previousTex0 = 0;
		glGetIntegerv(GL_ACTIVE_TEXTURE, &previousTexUnit);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTex);
		glActiveTexture(GL_TEXTURE0);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTex0);

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

		//Restore previous texture bound to GL_TEXTURE0, then previously active unit
		glBindTexture(GL_TEXTURE_2D, previousTex0);
		glActiveTexture(previousTexUnit);
		glBindTexture(GL_TEXTURE_2D, previousTex);
	}
};


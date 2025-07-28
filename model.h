#pragma once

#include <unordered_map>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "stb_image.h"
#include "shader.h"
#include "mesh.h"

class Model {
public:

	Model(std::string path) {
		loadModel(path);
	}

	void draw(Shader &shader) {
		for (unsigned int i = 0; i < meshes.size(); i++)
			meshes[i].draw(shader);
	}

private:

	std::vector<Mesh> meshes;
	std::string directory;
	std::unordered_map<std::string, Texture> loadedTextures;

	void loadModel(std::string path) {

		Assimp::Importer import;
		const aiScene *scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			std::cout << "ERROR:ASSIMP::" << import.GetErrorString() << std::endl;
			return;
		}

		directory = path.find_last_of('/');
		meshes.reserve(scene->mNumMeshes);
		processNode(scene->mRootNode, scene);
	}

	void processNode(aiNode *node, const aiScene *scene) {

		for (unsigned int i = 0; i < node->mNumMeshes; i++) {
			aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
			meshes.emplace_back(processMesh(mesh, scene));
		}

		for (unsigned int i = 0; i < node->mNumChildren; i++)
			processNode(node->mChildren[i], scene);
	}

	
	Mesh processMesh(aiMesh *mesh, const aiScene *scene) {

		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;
		std::vector<Texture> textures;

		vertices.reserve(mesh->mNumVertices);
		
		for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
			Vertex vertex;
			glm::vec3 vector;

			vector.x = mesh->mVertices[i].x;
			vector.y = mesh->mVertices[i].y;
			vector.z = mesh->mVertices[i].z;
			vertex.position = vector;

			vector.x = mesh->mNormals[i].x;
			vector.y = mesh->mNormals[i].y;
			vector.z = mesh->mNormals[i].z;
			vertex.normal = vector;

			if (mesh->mTextureCoords[0]) {
				glm::vec2 vec;
				vec.x = mesh->mTextureCoords[0][i].x;
				vec.y = mesh->mTextureCoords[0][i].y;
				vertex.texCoords = vec;
			}
			else
				vertex.texCoords = glm::vec2(0.0f);

			vertices.emplace_back(std::move(vertex));
		}
	
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
		
		if (mesh->mMaterialIndex >= 0) {
			aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
			std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, TEXTURE_DIFFUSE);
			std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, TEXTURE_SPECULAR);
			textures.reserve(diffuseMaps.size() + specularMaps.size());
			textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
			textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
		}

		return Mesh(std::move(vertices), std::move(indices), std::move(textures));
	}

	std::vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type,TextureType typeName) {

		std::vector<Texture> materialTextures;
		unsigned int numTextures = mat->GetTextureCount(type);
		materialTextures.reserve(numTextures);

		for (unsigned int i = 0; i < numTextures; i++) {
			aiString str;
			mat->GetTexture(type, i, &str);
			
			//Check unordered map before loading a texture from file
			if (loadedTextures.find(str.C_Str()) != loadedTextures.end())
				materialTextures.push_back(loadedTextures[str.C_Str()]);
			else {
				Texture texture;
				texture.id = TextureFromFile(str.C_Str(), directory);
				texture.type = typeName;
				texture.path = str.C_Str();
				materialTextures.push_back(texture);
			}
		}

		return materialTextures;
	}
	
	unsigned int TextureFromFile(char const *path, const std::string &directory) {

		unsigned int textureID;
		int width, height, nrChannels;

		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);

		std::string fullPath = directory + '/' + path;

		unsigned char *image = stbi_load(fullPath.c_str(), &width, &height, &nrChannels, 0);

		if (image) {
			GLenum imageFormat;

			if (nrChannels == 1)
				imageFormat = GL_RED;
			else if (nrChannels == 2)
				imageFormat = GL_RG;
			else if (nrChannels == 3)
				imageFormat = GL_RGB;
			else if (nrChannels == 4)
				imageFormat = GL_RGBA;

			glTexImage2D(GL_TEXTURE_2D, 0, imageFormat, width, height, 0, imageFormat, GL_UNSIGNED_BYTE, image);
			glGenerateMipmap(GL_TEXTURE_2D);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			stbi_image_free(image);
		}
		else {
			std::cout << "Error loading texture image from: " << fullPath << std::endl;
			stbi_image_free(image);

			GLubyte magenta[] = {255, 0, 255, 255};
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, magenta);
			glGenerateMipmap(GL_TEXTURE_2D);
		}

		glBindTexture(GL_TEXTURE_2D, 0);

		return textureID;
	}
};
#pragma once

#include <unordered_map>
#include <string>
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "stb_image.h"
#include "shader.h"
#include "mesh.h"

//Model owns the OpenGL Texture IDs and copying is disabled to avoid double frees.  Texture IDs are released on Model destruction
class Model {
public:

	Model(const Model &) = delete;
	Model &operator=(Model const&) = delete;

	Model(std::string path) {
		loadModel(path);
	}
	
	Model(Model &&other) noexcept :
		meshes(std::move(other.meshes)),
		directory(std::move(other.directory)),
		loadedTextures(std::move(other.loadedTextures)) {
		other.loadedTextures.clear();
	}

	Model &operator=(Model &&other) noexcept{
		if (this != &other) {
			
			for (const auto &pair : loadedTextures)
				glDeleteTextures(1, &pair.second.id);

			meshes = std::move(other.meshes);
			directory = std::move(other.directory);
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
		GLint previousVAO = 0;
		GLint previousTex = 0;
		
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTex); 
		
		for (unsigned int i = 0; i < meshes.size(); i++)
			meshes[i].draw(shader);

		//Restore
		glBindVertexArray(previousVAO);
		glActiveTexture(previousTex);
	}

private:

	std::vector<Mesh> meshes;
	std::string directory;
	std::unordered_map<std::string, Texture> loadedTextures;

	void loadModel(std::string path) {

		Assimp::Importer import;
		const aiScene *scene;

		std::string fileFormat = path.substr(path.find_last_of('.'));

		if (fileFormat == ".assbin")
			scene = import.ReadFile(path, 0);
		else
			scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			std::cout << "ERROR:ASSIMP::" << import.GetErrorString() << std::endl;
			return;
		}

		directory = path.substr(0, path.find_last_of('/') + 1);

		//Export it for next time
		if (fileFormat != ".assbin") {
			exportScene(scene, path);
		}

		meshes.reserve(scene->mNumMeshes);
		preloadTextures(scene);
		processNode(scene->mRootNode, scene);
	}

	//Fill "loadedTextures" map with all scene textures
	void preloadTextures(const aiScene *scene) {

		unsigned int totalTextures = 0;

		for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
			aiMaterial *material = scene->mMaterials[i];
			totalTextures += material->GetTextureCount(aiTextureType_DIFFUSE);
			totalTextures += material->GetTextureCount(aiTextureType_SPECULAR);
		}

		loadedTextures.reserve(totalTextures);

		for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
			aiMaterial *material = scene->mMaterials[i];
			loadMaterialTextures(material, aiTextureType_DIFFUSE, TEXTURE_DIFFUSE);
			loadMaterialTextures(material, aiTextureType_SPECULAR, TEXTURE_SPECULAR);
		}
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
		
		//Assign mesh-specific textures from loadedTextures map
		if (mesh->mMaterialIndex >= 0) {
			aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
			std::vector<Texture> diffuseMaps = texturesFromPreloaded(material, aiTextureType_DIFFUSE, TEXTURE_DIFFUSE);

			std::vector<Texture> specularMaps;
			if (material->GetTextureCount(aiTextureType_SPECULAR) > 0)
				specularMaps = texturesFromPreloaded(material, aiTextureType_SPECULAR, TEXTURE_SPECULAR);
			
			textures.reserve(diffuseMaps.size() + specularMaps.size());
			textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
			textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
		}

		return Mesh(std::move(vertices), std::move(indices), std::move(textures));
	}

	//Load texture from file if not already in loadedTextures map.  Todo: Multi-thread loading from storage
	void loadMaterialTextures(aiMaterial *mat, aiTextureType type,TextureType typeName) {

		for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
			aiString str;
			mat->GetTexture(type, i, &str);
			std::string name = str.C_Str();
			
			auto iterator = loadedTextures.find(name);

			if (iterator == loadedTextures.end()) {
				Texture texture;
				texture.id = TextureFromFile(name.c_str(), directory);
				texture.type = typeName;
				texture.path = name;
				loadedTextures.insert({ texture.path, texture });
			}
		}
	}
	
	//Returns an OpenGL Texture ID.  If texture image fails to load, a solid magenta color is assigned to that texture ID
	unsigned int TextureFromFile(char const *path, const std::string &directory) {

		unsigned int textureID;
		int width, height, nrChannels;
		std::string fullPath = directory + path;

		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);
		
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
			//stbi_image_free(image);

			GLubyte magenta[] = {255, 0, 255, 255};
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, magenta);
			glGenerateMipmap(GL_TEXTURE_2D);
		}

		glBindTexture(GL_TEXTURE_2D, 0);

		return textureID;
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
				textures.emplace_back(iterator->second.id, iterator->second.type, iterator->second.path);
			else
				std::cout << "ERROR: Texture not pre-loaded!  Texture name: " << name << std::endl;
		}

		return textures;
	}

	void exportScene(const aiScene *scene, std::string &path) {

		Assimp::Exporter exporter;
		std::string fileName = path.substr(path.find_last_of('/') + 1);
		fileName = fileName.substr(0, fileName.find_last_of('.'));
		exporter.Export(scene, "assbin", directory + fileName + ".assbin");

		std::cout << "Exported non .assbin file as \"" << fileName << ".assbin\" in: " << directory << std::endl;
	}
};
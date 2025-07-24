#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
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

	void loadModel(std::string path) {
		Assimp::Importer import;
		const aiScene *scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
			std::cout << "ERROR:ASSIMP::" << import.GetErrorString() << std::endl;
			return;
		}

		directory = path.find_last_of('/');
		processNode(scene->mRootNode, scene);
	}

	void processNode(aiNode *node, const aiScene *scene) {}

	
	Mesh processMesh(aiMesh *mesh, const aiScene *scene) {}

	std::vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName) {}
	
};
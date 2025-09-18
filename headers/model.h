#pragma once

#include "texture.h"
#include "shader.h"
#include "primitive.h"
#include <fastgltf/types.hpp>
#include <unordered_map>
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>
#include <glad/glad.h>

//Model owns the OpenGL Texture IDs and copying is disabled to avoid double frees.  Texture IDs are released on Model destruction.
//assetImages is a 1:1 mapping of gltf's images array, but with OpenGL texture ID's instead of URI's.
//materials is a 1:1 mapping of gltf's materials array
class Model {
public:
	struct Material {
		unsigned int assetImageIndex;
		TextureType type;

		Material(unsigned int index = 0, TextureType t = TEXTURE_NONE) : assetImageIndex(index), type(t) {};
	};

	Model(std::string path);
	Model(Model&& other) noexcept;
	Model& operator=(Model&& other) noexcept;
	Model(const Model&) = delete;
	Model& operator=(Model const&) = delete;
	~Model();

	void draw(Shader& shader);

private:
	std::vector<Primitive::Mesh> meshes;
	std::vector<Material> materials;
	std::vector<GLuint> assetImages;

	void loadModel(std::filesystem::path path);
	void preloadTextures(const fastgltf::Asset& asset, const std::filesystem::path parentPath);
	void generateTextureIDs(std::unordered_map<std::filesystem::path, unsigned int> texturePaths);
	void assembleTexureInfo(TextureType type, GLuint id);
};


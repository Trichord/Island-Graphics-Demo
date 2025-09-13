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

//Model owns the OpenGL Texture IDs and copying is disabled to avoid double frees.  Texture IDs are released on Model destruction
class Model {
public:
	Model(std::string path);
	Model(Model&& other) noexcept;
	Model& operator=(Model&& other) noexcept;
	Model(const Model&) = delete;
	Model& operator=(Model const&) = delete;
	~Model();

	void draw(Shader& shader);

private:
	std::vector<Primitive::Mesh> meshes;
	std::unordered_map<std::filesystem::path, TextureType> texturePaths;
	std::array<GLuint, 4> textureIDs;

	void loadModel(std::filesystem::path path);
	void preloadTextures(const fastgltf::Asset& asset, const std::filesystem::path parentPath);
	void generateTextureIDs();
	void assembleTexureInfo(TextureType type, GLuint id);
};


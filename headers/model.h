#pragma once

#include "texture.h"
#include "shader.h"
#include "primitive.h"
#include "image_loader.h"
#include <fastgltf/types.hpp>
#include <unordered_map>
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>
#include <glad/glad.h>

/* 
Model owns the OpenGL Texture IDs and copying is disabled to avoid double frees. Texture IDs are released on Model destruction. 
assetImages is a 1:1 mapping of gltf's images array, but with OpenGL texture IDs instead of URIs.  Order of fastgltf's images is preserved
materials is a 1:1 mapping of gltf's materials array to the assetImages texture IDs 
*/

class Model {
public:
	struct Material {
		unsigned int albedoImageIndex, metallicRoughnessImageIndex, normalImageIndex, aoImageIndex;

		Material(unsigned int alb = 0, unsigned int metRough = 0, unsigned int norm = 0, unsigned int ao = 0) : albedoImageIndex(alb), metallicRoughnessImageIndex(metRough), normalImageIndex(norm), aoImageIndex(ao) {};
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
	void loadMaterials(const fastgltf::Asset& asset);
	void loadTextures(const fastgltf::Asset& asset, const std::filesystem::path parentPath);
	std::vector<std::filesystem::path> getURIs(const fastgltf::Asset& asset, const std::filesystem::path parentPath);
	void generateTextureID(const ImageData& image, bool sRGB);
};


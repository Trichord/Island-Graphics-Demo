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
samplers takes the "source" from gltf (an image index in the image array) as a key and maps it to the correct sampler options
*/

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
	struct Material {
		unsigned int albedoImageIndex, metallicRoughnessImageIndex, normalImageIndex, aoImageIndex;

		Material(unsigned int alb = 0, unsigned int metRough = 0, unsigned int norm = 0, unsigned int ao = 0) : albedoImageIndex(alb), metallicRoughnessImageIndex(metRough), normalImageIndex(norm), aoImageIndex(ao) {};
	};

	struct Sampler {
		std::uint16_t magFilter, minFilter, wrapS, wrapT;

		Sampler(uint16_t mag = GL_LINEAR, uint16_t min = GL_LINEAR_MIPMAP_LINEAR, uint16_t ws = GL_REPEAT, uint16_t wt = GL_REPEAT) : magFilter(mag), minFilter(min), wrapS(ws), wrapT(wt) {};
	};
	std::vector<Primitive::Mesh> meshes;
	std::vector<Material> materials;
	std::vector<GLuint> assetImages;

	void loadModel(std::filesystem::path path);
	void loadTextures(const fastgltf::Asset& asset, const std::filesystem::path parentPath);
	void getMaterials(const fastgltf::Asset& asset, const std::vector<ImageData>& imagedata);
	std::vector<std::filesystem::path> getURIs(const fastgltf::Asset& asset, const std::filesystem::path parentPath);
	void generateTextureID(const ImageData& image, unsigned int index, bool sRGB);
};


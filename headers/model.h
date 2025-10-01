#pragma once

#include "shader.h"
#include "primitive.h"
#include "image_loader.h"
#include "renderable.h"
#include <fastgltf/types.hpp>
#include <unordered_set>
#include <vector>
#include <filesystem>
#include <glad/glad.h>

/* 
Model owns the OpenGL Texture IDs and copying is disabled to avoid double frees. Texture IDs are released on Model destruction. 
assetImages is a 1:1 mapping of gltf's images array, but with OpenGL texture IDs instead of URIs.  Order of fastgltf's images is preserved
*/

class Model : public Renderable{
public:
	Model(std::string path);
	Model(Model&& other) noexcept;
	Model& operator=(Model&& other) noexcept;
	Model(const Model&) = delete;
	Model& operator=(const Model&) = delete;
	~Model();

	void render(Shader& shader);

private:
	struct Material {
		unsigned int albedoImageIndex, aoMetallicRoughnessImageIndex, normalImageIndex;

		Material(unsigned int alb = 0, unsigned int metRough = 0, unsigned int norm = 0, unsigned int ao = 0) : albedoImageIndex(alb), aoMetallicRoughnessImageIndex(metRough), normalImageIndex(norm) {};
	};

	fastgltf::Asset asset;
	std::vector<Primitive::Mesh> meshes;
	std::vector<Material> materials;
	std::vector<GLuint> assetImages;
	std::unordered_set<unsigned int> processedImageIndices;

	void loadModel(std::filesystem::path path);
	void loadTextures(const std::filesystem::path parentPath);
	void loadMaterials(const std::vector<ImageData>& imagedata);
	void deleteTextureIDs();
};


#pragma once

#include "cube_mapped_model.h"
#include "primitive.h"
#include "shader.h"
#include <vector>
#include <filesystem>

class SpaceModel : public CubeMappedModel {
public:
	SpaceModel(std::filesystem::path path);
	void draw(Shader &shader) override;

protected:
	std::vector<Primitive::Mesh> meshes;
	void loadModel(std::filesystem::path& path);
};
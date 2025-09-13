#pragma once

#include "../headers/cube_mapped_model.h"
#include "../src/primitive.h"
#include <vector>
#include <filesystem>

class SpaceModel : public CubeMappedModel {
public:
	SpaceModel(std::filesystem::path path);
	void draw() override;

protected:
	std::vector<Primitive::Mesh> meshes;
	void loadModel(std::filesystem::path& path);
};
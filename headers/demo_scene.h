#pragma once

#include "scene.h"
#include "renderable.h"
#include "model.h"
#include "skybox.h"
#include "space_model.h"
#include <vector>

//Concrete implementation of a scene
class DemoScene : public Scene {
public:
	DemoScene();
private:
	std::vector<Model> models;
	std::vector<SpaceModel> cubeMappedModels;
	std::vector<Skybox> skyboxes;
	
	void loadModels();
};

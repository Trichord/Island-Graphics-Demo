#include "../headers/demo_scene.h"
#include "../headers/model.h"
#include "../headers/space_model.h"
#include "../headers/skybox.h"

DemoScene::DemoScene() {
	camera = glm::vec3(0.0f, 3.0f, 30.0f);
	loadModels();
}

void DemoScene::loadModels() {
	models.emplace_back("./models/island/island.gltf");
	models.emplace_back("./models/flight_helmet/FlightHelmet.gltf");
	cubeMappedModels.emplace_back("./models/dodecahedron/scene.gltf");
	
	std::vector<std::string> skyBoxPaths = {
		"./textures/skybox/right.png",
			"./textures/skybox/left.png",
			"./textures/skybox/top.png",
			"./textures/skybox/bottom.png",
			"./textures/skybox/front.png",
			"./textures/skybox/back.png"
	};
	
	skyboxes.emplace_back(skyBoxPaths);


}
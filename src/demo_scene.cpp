#include "../headers/demo_scene.h"
#include "../headers/model.h"
#include "../headers/space_model.h"
#include "../headers/skybox.h"
#include <functional>

DemoScene::DemoScene() {
	//Normalized approximation of sun direction in skybox
	globalLightDirection = glm::vec3(-0.363647, -0.739629, -0.566312);
	
	camera = glm::vec3(0.0f, 3.0f, 30.0f);
	loadModels();
}

void DemoScene::loadModels() {
	//Main island
	models.emplace_back("./models/island/island.gltf");
	sceneObjects[ShaderType::DIRECTIONAL_PBR].emplace_back(
		RenderableObject{ std::ref(models.back()), Transform() }
	);

	//Flight helmet
	models.emplace_back("./models/flight_helmet/FlightHelmet.gltf");
	sceneObjects[ShaderType::POINT_PBR].emplace_back(
		RenderableObject{ std::ref(models.back()), Transform(glm::vec3(0.0f, -2.3f, 50.0f), 180.0f, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(3.0f)) }
	);

	//Fancy reflective space dodecahedron
	cubeMappedModels.emplace_back("./models/dodecahedron/scene.gltf");
	sceneObjects[ShaderType::REFLECTIVE].emplace_back(
		RenderableObject{ std::ref(cubeMappedModels.back()), Transform(glm::vec3(-21.0f, 16.0f, -25.0f)) }
	);
	
	//Skybox
	std::vector<std::string> skyBoxPaths = {
		"./textures/skybox/right.png",
			"./textures/skybox/left.png",
			"./textures/skybox/top.png",
			"./textures/skybox/bottom.png",
			"./textures/skybox/front.png",
			"./textures/skybox/back.png"
	};
	
	skyboxes.emplace_back(skyBoxPaths);

	sceneObjects[ShaderType::SKYBOX].emplace_back(
		RenderableObject { std::ref(skyboxes.back()), Transform()}
	);
}
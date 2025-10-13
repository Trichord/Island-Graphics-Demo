#include "../headers/demo_scene.h"

#include <assert.h>
#include <functional>
#include <iostream>

#include "../headers/model.h"
#include "../headers/skybox.h"
#include "../headers/space_model.h"

DemoScene::DemoScene() {

  camera = glm::vec3(0.0f, 3.0f, 30.0f);
  std::cout << "Loading Models";
  loadAllModels();
  std::cout << " Done." << std::endl;
  addLights();  
}

void DemoScene::loadAllModels() {
  // CRITCIAL: Reserve space for total number of models in each vector
  const int numModels = 2;
  const int numCubeMappedModels = 1;
  const int numSkyboxes = 1;
  models.reserve(numModels);
  spaceModels.reserve(numCubeMappedModels);
  skyboxes.reserve(numSkyboxes);

  //Main island
  auto& island = loadModel("./models/island/island.gltf");
  addObjectToScene(island, ShaderType::DIRECTIONAL_PBR, Transform());

  std::cout << ".";

  //Flight helmet
  auto& helmet = loadModel("./models/flight_helmet/FlightHelmet.gltf");
  addObjectToScene(helmet, ShaderType::POINT_PBR,
                   Transform(glm::vec3(0.0f, -2.3f, 50.0f), 180.0f,
                             glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(3.0f)));

  std::cout << ".";

  //Fancy spinning space dodecahedron
  auto& spaceDodeca = loadSpaceModel("./models/dodecahedron/scene.gltf");
  addObjectToScene(spaceDodeca, ShaderType::REFLECTIVE,
                   Transform(glm::vec3(-21.0f, 16.0f, -25.0f)), "spaceDodeca");
  
  std::cout << ".";

  //Skybox
  std::vector<std::string> skyBoxPaths = {
      "./textures/skybox/right.png", "./textures/skybox/left.png",
      "./textures/skybox/top.png",   "./textures/skybox/bottom.png",
      "./textures/skybox/front.png", "./textures/skybox/back.png"};

  auto& skybox = loadSkybox(skyBoxPaths);
  addObjectToScene(skybox, ShaderType::SKYBOX, Transform());

  //For development only
  assert(models.size() <= numModels);
  assert(cubeMappedModels.size() <= numCubeMappedModels);
  assert(skyboxes.size() <= numSkyboxes);
}

void DemoScene::addLights() {
  // Normalized approximation of sun direction in skybox
  directionalLights.emplace_back(glm::vec3(-0.363647, -0.739629, -0.566312));

  // Right next to the flight helmet
  pointLights.emplace_back(glm::vec3(0.0f, 0.5f, 50.0f));
}
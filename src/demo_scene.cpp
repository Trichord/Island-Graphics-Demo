#include "../headers/demo_scene.h"
#include "../headers/model.h"
#include "../headers/skybox.h"
#include "../headers/space_model.h"
#include "../headers/object_positions.h"

#include <functional>
#include <iostream>

DemoScene::DemoScene() { initializeScene(); }

void DemoScene::addAllModelsToScene() {
  std::cout << "Loading models";

  // Main island
  auto& island = loadModel("./models/island/island.gltf");
  addObjectToScene(island, ShaderType::DIRECTIONAL_PBR, Transform());

  std::cout << ".";

  // Flight helmet
  auto& helmet = loadModel("./models/flight_helmet/FlightHelmet.gltf");
  addObjectToScene(helmet, ShaderType::POINT_PBR,
                   Transform(FLIGHT_HELMET_START, glm::vec3(3.0f), 180.0f,
                             glm::vec3(0.0f, 1.0f, 0.0f)));

  std::cout << ".";

  //Lightbulb
  auto& lightbulb = loadModel("./models/lightbulb/lightbulb.gltf");
  addObjectToScene(
      lightbulb, ShaderType::LIGHT_SOURCE,
      Transform(LIGHTBULB_START, glm::vec3(1.0f), 90.0f, glm::vec3(1.0f, 0.0f, 0.0f)), "lightbulb");

  // Fancy spinning space dodecahedron
  auto& spaceDodeca = loadSpaceModel("./models/dodecahedron/scene.gltf");
  addObjectToScene(spaceDodeca, ShaderType::REFLECTIVE,
                   Transform(glm::vec3(-21.0f, 16.0f, -25.0f)), "spaceDodeca");

  std::cout << ".";

  // Skybox
  std::vector<std::string> skyBoxPaths = {
      "./textures/skybox/right.png", "./textures/skybox/left.png",
      "./textures/skybox/top.png",   "./textures/skybox/bottom.png",
      "./textures/skybox/front.png", "./textures/skybox/back.png"};

  auto& skybox = loadSkybox(skyBoxPaths);
  addObjectToScene(skybox, ShaderType::SKYBOX, Transform());

  //Grass
  auto& grass = loadGrass("./res/grass1.txt", "./textures/grass.png", 0.3f, 0.5f, 3, 8109);
  addObjectToScene(grass, ShaderType::GRASS, Transform());

  std::cout << std::endl;
}

void DemoScene::addAllLightsToScene() {
  // Normalized approximation of sun direction in skybox
  addLightToScene(directionalLights,
                  DirectionalLight{glm::vec3(-0.363647, -0.739629, -0.566312)});

  // Right next to the flight helmet
  addLightToScene(pointLights, PointLight{LIGHTBULB_START},
                  "circling_light");
}

void DemoScene::setupCamera() { camera = glm::vec3(0.0f, 3.0f, 30.0f); }
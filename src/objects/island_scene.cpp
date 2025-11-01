#include "island_scene.h"

#include <functional>
#include <iostream>

#include "model.h"
#include "object_positions.h"

IslandScene::IslandScene() { initializeScene(); }

void IslandScene::addAllModelsToScene() {
  std::cout << "Loading models";

  // Main island
  auto& island = loadModel("./res/models/island/island.gltf");
  addObjectToScene(island, ShaderType::DIRECTIONAL_PBR);

  std::cout << ".";

  // Flight helmet
  auto& helmet = loadModel("./res/models/flight_helmet/FlightHelmet.gltf");
  addObjectToScene(helmet, ShaderType::POINT_PBR,
                   Transform(FLIGHT_HELMET_START, glm::vec3(3.0f), 180.0f,
                             glm::vec3(0.0f, 1.0f, 0.0f)));

  std::cout << ".";

  // Lightbulb-
  auto& lightbulb = loadModel("./res/models/lightbulb/lightbulb.gltf");
  addObjectToScene(lightbulb, ShaderType::LIGHT_SOURCE,
                   Transform(LIGHTBULB_START, glm::vec3(1.0f), 90.0f,
                             glm::vec3(1.0f, 0.0f, 0.0f)),
                   "lightbulb");

  std::vector<std::filesystem::path> spaceBoxPaths{
      "./res//Textures/nebula/right.png", "./res//Textures/nebula/left.png",
      "./res//Textures/nebula/top.png",   "./res//Textures/nebula/bottom.png",
      "./res//Textures/nebula/front.png", "./res//Textures/nebula/back.png"};

  // Fancy spinning space dodecahedron
  auto& spaceDodeca =
      loadSpaceModel("./res/models/dodecahedron/scene.gltf", spaceBoxPaths);
  addObjectToScene(spaceDodeca, ShaderType::REFLECTIVE,
                   Transform(glm::vec3(-21.0f, 16.0f, -25.0f)), "spaceDodeca");

  std::cout << ".";

  // Skybox
  std::vector<std::filesystem::path> skyBoxPaths{
      "./res/textures/skybox/right.png", "./res/textures/skybox/left.png",
      "./res/textures/skybox/top.png",   "./res/textures/skybox/bottom.png",
      "./res/textures/skybox/front.png", "./res/textures/skybox/back.png"};

  auto& skybox = loadSkybox(skyBoxPaths);
  addObjectToScene(skybox, ShaderType::SKYBOX);

  // Grass
  auto& grass = loadQuad("./res/grass1.txt", "./res/textures/grass1.png",
                         Quad::RADIAL_PATTERN, 0.3f, 0.5f, 3, 8109);
  addObjectToScene(grass, ShaderType::GRASS);

  // Flowers
  auto& flower1 = loadInstancedModel("./res/models/purpleflower/purpleflower.gltf",
                                     "./res/flowers1.txt", 317);
  addObjectToScene(flower1, ShaderType::FLOWERS);

  std::cout << " Done." << std::endl;
}

void IslandScene::addAllLightsToScene() {
  // Normalized approximation of sun direction in skybox
  globalLightSource = glm::vec3{-0.363647, -0.739629, -0.566312};

  // Right next to the flight helmet
  addLightToScene(pointLights, PointLight{LIGHTBULB_START}, "circlingLight");
}

void IslandScene::setupCamera() { camera = glm::vec3{0.0f, 3.0f, 30.0f}; }
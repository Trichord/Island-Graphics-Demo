#include "island_scene.h"

#include <functional>
#include <iostream>

#include "model.h"
#include "object_positions.h"

IslandScene::IslandScene() { initializeScene(); }

void IslandScene::addAllModelsToScene() {
  using enum ShaderType;
  std::cout << "Loading models";

  // Main island
  auto& island = loadModel("./res/models/island/island.gltf");
  addObjectToScene(island, DIRECTIONAL_PBR);

  std::cout << ".";

  // Flight helmet
  auto& helmet = loadModel("./res/models/flight_helmet/FlightHelmet.gltf");
  addObjectToScene(helmet, POINT_PBR,
                   Transform(FLIGHT_HELMET_START, glm::vec3(3.0f), 180.0f,
                             glm::vec3(0.0f, 1.0f, 0.0f)));

  std::cout << ".";

  // Lightbulb-
  auto& lightbulb = loadModel("./res/models/lightbulb/lightbulb.gltf");
  addObjectToScene(lightbulb, LIGHT_SOURCE,
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
  addObjectToScene(spaceDodeca, REFLECTIVE,
                   Transform(glm::vec3(-21.0f, 16.0f, -25.0f)), "spaceDodeca");

  std::cout << ".";

  // Skybox
  std::vector<std::filesystem::path> skyBoxPaths{
      "./res/textures/skybox/right.png", "./res/textures/skybox/left.png",
      "./res/textures/skybox/top.png",   "./res/textures/skybox/bottom.png",
      "./res/textures/skybox/front.png", "./res/textures/skybox/back.png"};

  auto& skybox = loadSkybox(skyBoxPaths);
  addObjectToScene(skybox, SKYBOX);

  // Grass
  auto& grass = loadQuad("./res/textures/grass.png", "./res/grass_pos.txt",
                         Quad::RADIAL_PATTERN, 0.3f, 0.5f, 3, 8109);
  addObjectToScene(grass, FLOWERS);

  // Purple flowers
  auto& purpleFlowers =
      loadInstancedModel("./res/models/purpleflower/purpleflower.gltf",
                         "./res/purpleflower_pos.txt", 0.9f, 1.1f, 475);
  addObjectToScene(purpleFlowers, FLOWERS);

  // White flowers
  auto& whiteFlowers =
      loadInstancedModel("./res/models/whiteflower/whiteflower.gltf",
                         "./res/whiteflower_pos.txt", 0.9f, 1.1f, 417);
  addObjectToScene(whiteFlowers, FLOWERS);

  // Blue flowers
  auto& blueFlowers =
      loadInstancedModel("./res/models/blueflower/blueflower.gltf",
                         "./res/blueflower_pos.txt", 0.9f, 1.1f, 361);
  addObjectToScene(blueFlowers, FLOWERS);

  // Fir Tree
  auto& tree = loadInstancedModel("./res/models/firtree/firtree.gltf",
                                  "./res/elm_positions.txt", 0.8f, 1.25f, 24);
  addObjectToScene(tree, TREES);

  std::cout << " Done." << std::endl;
}

void IslandScene::addAllLightsToScene() {
  // Normalized approximation of sun direction in skybox
  globalLightSource = glm::vec3{-0.363647, -0.739629, -0.566312};

  // Right next to the flight helmet
  addLightToScene(pointLights, PointLight{LIGHTBULB_START}, "circlingLight");
}

void IslandScene::setupCamera() { camera = glm::vec3{0.0f, 3.0f, 30.0f}; }
#include "island_scene.h"

#include "model.h"
#include "object_positions.h"
#include "texture_manager.h"

IslandScene::IslandScene(TextureManager& textureManager)
    : Scene(textureManager) {
  initializeScene();
}

void IslandScene::addAllModelsToScene() {
  using enum ShaderType;

  // Main island
  auto& island = loadModel("./res/models/island/island.gltf");
  addObjectToScene(island, DIRECTIONAL_PBR);

    // Dead tree
  auto& deadTree = loadModel("./res/models/dead_tree/dead_tree.gltf");
  addObjectToScene(deadTree, DIRECTIONAL_PBR,
                   Transform(DEAD_TREE_POS, glm::vec3{1.0f}, 45.0f));

  // Flight helmet
  auto& helmet = loadModel("./res/models/flight_helmet/FlightHelmet.gltf");
  addObjectToScene(helmet, POINT_PBR,
                   Transform(FLIGHT_HELMET_POS, glm::vec3(3.0f), 180.0f,
                             glm::vec3(0.0f, 1.0f, 0.0f)));

  // Lightbulb-
  auto& lightbulb = loadModel("./res/models/lightbulb/lightbulb.gltf");
  addObjectToScene(lightbulb, LIGHT_SOURCE,
                   Transform(LIGHTBULB_POS, glm::vec3(1.0f), 90.0f,
                             glm::vec3(1.0f, 0.0f, 0.0f)),
                   "lightbulb");

  std::vector<std::filesystem::path> spaceBoxPaths{
      "./res//Textures/nebula/right.png", "./res//Textures/nebula/left.png",
      "./res//Textures/nebula/top.png",   "./res//Textures/nebula/bottom.png",
      "./res//Textures/nebula/front.png", "./res//Textures/nebula/back.png"};

  // Fancy spinning space model
  auto& spaceModel =
      loadCubeMappedModel("./res/models/space_model/scene.gltf", spaceBoxPaths);
  addObjectToScene(spaceModel, REFLECTIVE, Transform(SPACE_DODECA_POS),
                   "spaceModel");

  // Skybox
  std::vector<std::filesystem::path> skyBoxPaths{
      "./res/textures/skybox/right.png", "./res/textures/skybox/left.png",
      "./res/textures/skybox/top.png",   "./res/textures/skybox/bottom.png",
      "./res/textures/skybox/front.png", "./res/textures/skybox/back.png"};

  auto& skybox = loadSkybox(skyBoxPaths);
  addObjectToScene(skybox, SKYBOX);

  // Grass
  auto& grass = loadInstancedModel("./res/models/grass/grass.gltf",
                                   "./res/grass.txt", 0.3f, 0.5, 905);
  addObjectToScene(grass, FLOWERS);

  // Dandelions
  auto& dandelions =
      loadInstancedModel("./res/models/dandelion/dandelion.gltf",
                         "./res/dandelions.txt", 1.0f, 1.5f, 1887);
  addObjectToScene(dandelions, FLOWERS);

  // Purple flowers
  auto& purpleFlowers =
      loadInstancedModel("./res/models/purpleflower/purpleflower.gltf",
                         "./res/purpleflowers.txt", 0.9f, 1.1f, 256);
  addObjectToScene(purpleFlowers, FLOWERS);

  // White flowers
  auto& whiteFlowers =
      loadInstancedModel("./res/models/whiteflower/whiteflower.gltf",
                         "./res/whiteflowers.txt", 0.9f, 1.1f, 371);
  addObjectToScene(whiteFlowers, FLOWERS);

  // Blue flowers
  auto& blueFlowers = loadInstancedModel(
      whiteFlowers, "./res/models/blueflower/blueflower.gltf",
      "./res/blueflowers.txt", 0.9f, 1.1f, 181);
  addObjectToScene(blueFlowers, FLOWERS);

  // Tree 1
  auto& tree1 = loadInstancedModel("./res/models/tree1/tree1.gltf",
                                   "./res/tree1.txt", 0.8f, 1.2f, 9);
  addObjectToScene(tree1, TREES);

  // Tree 2
  auto& tree2 = loadInstancedModel(tree1, "./res/models/tree3/tree3.gltf",
                                   "./res/tree2.txt", 0.8f, 1.125f, 67);
  addObjectToScene(tree2, TREES);

  /*
  // Noble trees
  auto& nobleFirTree =
      loadInstancedModel(tree1, "./res/models/noblefir/noblefir.gltf",
                         "./res/noble_firs.txt", 0.7f, 1.2f, 55);
  addObjectToScene(nobleFirTree, TREES);
  */
}

void IslandScene::addAllLightsToScene() {
  // Normalized approximation of sun direction in skybox
  globalLightSource = glm::vec3{-0.36669f, -0.731219f, -0.575202f};

  // Right next to the flight helmet
  addLightToScene(pointLights, PointLight{LIGHTBULB_POS}, "circlingLight");
}

void IslandScene::setupCamera() { camera = glm::vec3{0.0f, 0.0f, 30.0f}; }
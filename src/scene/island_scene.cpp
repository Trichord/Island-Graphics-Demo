#include "island_scene.h"
#include "texture_manager.h"

#include <functional>

#include "model.h"
#include "object_positions.h"

IslandScene::IslandScene(TextureManager& textureManager) : Scene(textureManager) { initializeScene(); }

void IslandScene::addAllModelsToScene() {
  using enum ShaderType;

  // Main island
  auto& island = loadModel("./res/models/island/island.gltf");
  addObjectToScene(island, DIRECTIONAL_PBR);

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
      loadSpaceModel("./res/models/space_model/scene.gltf", spaceBoxPaths);
  addObjectToScene(spaceModel, REFLECTIVE,
                   Transform(SPACE_DODECA_POS), "spaceModel");

  // Skybox
  std::vector<std::filesystem::path> skyBoxPaths{
      "./res/textures/skybox/right.png", "./res/textures/skybox/left.png",
      "./res/textures/skybox/top.png",   "./res/textures/skybox/bottom.png",
      "./res/textures/skybox/front.png", "./res/textures/skybox/back.png"};

  auto& skybox = loadSkybox(skyBoxPaths);
  addObjectToScene(skybox, SKYBOX);

  // Grass
  auto &grass = loadInstancedModel("./res/models/grass/grass.gltf", "./res/grass.txt", 0.3f,
                     0.5, 6881);
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
  auto& blueFlowers =
      loadInstancedModel(whiteFlowers, "./res/models/blueflower/blueflower.gltf",
                         "./res/blueflowers.txt", 0.9f, 1.1f, 181);
  addObjectToScene(blueFlowers, FLOWERS);

  // Fir trees
  auto& firTree = loadInstancedModel("./res/models/firtree/firtree.gltf",
                                     "./res/fir_trees.txt", 0.7f, 1.5f, 219);
  addObjectToScene(firTree, TREES);

  // Noble trees
  auto& nobleFirTree =
      loadInstancedModel(firTree, "./res/models/noblefir/noblefir.gltf",
                         "./res/noble_firs.txt", 0.7f, 1.2f, 55);
  addObjectToScene(nobleFirTree, TREES);

  // Dead trees
  auto& deadTrees =
      loadInstancedModel("./res/models/dead_tree/dead_tree.gltf",
                         "./res/dead_trees.txt", 0.65f, 1.25f, 23);
  addObjectToScene(deadTrees, TREES);

}

void IslandScene::addAllLightsToScene() {
  // Normalized approximation of sun direction in skybox
  globalLightSource = glm::vec3{-0.36669f, -0.731219f, -0.575202f};

  // Right next to the flight helmet
  addLightToScene(pointLights, PointLight{LIGHTBULB_POS}, "circlingLight");
}

void IslandScene::setupCamera() { camera = glm::vec3{0.0f, 3.0f, 30.0f}; }
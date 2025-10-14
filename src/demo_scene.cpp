#include "../headers/demo_scene.h"

#include <assert.h>
#include <functional>
#include <iostream>

#include "../headers/model.h"
#include "../headers/skybox.h"
#include "../headers/space_model.h"
#include "../headers/object_positions.h"

DemoScene::DemoScene() {
  camera = glm::vec3(0.0f, 3.0f, 30.0f);
  std::cout << "Loading Models";
  loadAllModels();
  std::cout << " Done." << std::endl;
  addLights();
}

void DemoScene::loadAllModels() {
  // CRITCIAL: Reserve space for total number of models in each vector
  const int numModels = 3;
  const int numCubeMappedModels = 1;
  const int numSkyboxes = 1;
  const int numGrassVariations = 1;
  grasses.reserve(numGrassVariations);
  models.reserve(numModels);
  spaceModels.reserve(numCubeMappedModels);
  skyboxes.reserve(numSkyboxes);

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
  auto& grass = loadGrass("./textures/grass.png");
  addObjectToScene(grass, ShaderType::GRASS, Transform());

  // For development only
  assert(models.size() <= numModels);
  assert(cubeMappedModels.size() <= numCubeMappedModels);
  assert(skyboxes.size() <= numSkyboxes);
  assert(grasses.size() <= numGrassVariations);
}

// Pre-allocate all space in vector before adding
void DemoScene::addLights() {
  const int numPointLights = 1;
  const int numDirLights = 1;

  directionalLights.reserve(numDirLights);
  pointLights.reserve(numPointLights);

  // Normalized approximation of sun direction in skybox
  addLightToScene(directionalLights,
                  DirectionalLight{glm::vec3(-0.363647, -0.739629, -0.566312)});

  // Right next to the flight helmet
  addLightToScene(pointLights, PointLight{LIGHTBULB_START},
                  "circling_light");

  assert(directionalLights.size() <= numDirLights);
  assert(pointLights.size() <= numPointLights);
}
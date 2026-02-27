#include "island_scene.h"

#include <filesystem>
#include <iostream>
#include <vector>

#include "camera.h"
#include "config_window.h"
#include "glm/glm.hpp"
#include "object_positions.h"
#include "renderable.h"
#include "scene.h"
#include "shader_type.h"
#include "texture_manager.h"

IslandScene::IslandScene(TextureManager &textureManager, const ConfigWindow::GraphicsSettings &graphicsSettings)
    : Scene(textureManager)
{
    initializeScene(graphicsSettings);
}

void IslandScene::initializeScene(const ConfigWindow::GraphicsSettings &graphicsSettings)
{
    setupCamera(graphicsSettings);
    std::cout << "Loading Models...";
    addAllModelsToScene();
    std::cout << " Done." << std::endl;
    addAllLightsToScene();
}

void IslandScene::addAllModelsToScene()
{
    using enum ShaderType;
    using enum CullMethod;

    /*****************
     *               *
     * Main Features *
     *               *
     *****************/

    // Main island
    auto &island = sceneResources_.loadModel("./res/models/island/island.gltf");
    addObjectToScene(island, DIRECTIONAL_PBR, {.cullMethod = TERRAIN});

    // Statue
    auto &statue = sceneResources_.loadModel("./res/models/set_pieces/statue/statue.gltf");
    addObjectToScene(statue, DIRECTIONAL_PBR,
                     {.transform = Transform{STATUE_POSITION, glm::vec3{1.0f}, -65.0f, glm::vec3{0.0f, 1.0f, 0.0f}}});

    std::vector<std::filesystem::path> spaceBoxPaths{
        "./res//Textures/nebula/right.png",  "./res//Textures/nebula/left.png",  "./res//Textures/nebula/top.png",
        "./res//Textures/nebula/bottom.png", "./res//Textures/nebula/front.png", "./res//Textures/nebula/back.png"};

    // Fancy spinning space model
    auto &spaceModel =
        sceneResources_.loadCubeMappedModel("./res/models/set_pieces/space_model/scene.gltf", spaceBoxPaths);
    addObjectToScene(spaceModel, REFLECTIVE, {.transform = {SPACE_DODECA_POS}}, "spaceModel");

    /**************
     *            *
     * Vegetation *
     *            *
     **************/

    // Grass
    auto &grass = sceneResources_.loadInstancedModel("./res/models/vegetation/grass/grass.gltf",
                                                    "./res/positions/grass.txt", 0.3f, 0.5, 1497);
    addObjectToScene(grass, FLOWERS, {.cullMethod = INSTANCED});

    // White flowers
    auto &whiteFlowers = sceneResources_.loadInstancedModel("./res/models/vegetation/whiteflower/whiteflower.gltf",
                                                           "./res/positions/whiteflower.txt", 0.9f, 1.1f, 488);
    addObjectToScene(whiteFlowers, FLOWERS, {.cullMethod = INSTANCED});
    ModelHandle whiteFlowerHandle{whiteFlowers.getHandle()};

    // Blue flowers
    auto &blueFlowers =
        sceneResources_.loadInstancedModel(whiteFlowerHandle, "./res/models/vegetation/blueflower/blueflower.gltf",
                                          "./res/positions/blueflower.txt", 0.9f, 1.1f, 212);
    addObjectToScene(blueFlowers, FLOWERS, {.cullMethod = INSTANCED});

    // Purple flowers
    auto &purpleFlowers =
        sceneResources_.loadInstancedModel(whiteFlowerHandle, "./res/models/vegetation/purpleflower/purpleflower.gltf",
                                          "./res/positions/purpleflower.txt", 0.9f, 1.1f, 328);
    addObjectToScene(purpleFlowers, FLOWERS, {.cullMethod = INSTANCED});

    // Dandelions
    auto &dandelions =
        sceneResources_.loadInstancedModel(whiteFlowerHandle, "./res/models/vegetation/dandelion/dandelion.gltf",
                                          "./res/positions/dandelion.txt", 1.0f, 1.5f, 944);
    addObjectToScene(dandelions, FLOWERS, {.cullMethod = INSTANCED});

    // Strawberry flowers
    auto &strawberries =
        sceneResources_.loadInstancedModel(whiteFlowerHandle, "./res/models/vegetation/strawberry/strawberry.gltf",
                                          "./res/positions/strawberry.txt", 0.8f, 1.2f, 120);
    addObjectToScene(strawberries, FLOWERS, {.cullMethod = INSTANCED});

    /*********
     *       *
     * Trees *
     *       *
     *********/

    // Tree 1
    auto &tree1 = sceneResources_.loadInstancedModel("./res/models/trees/tree1/tree1.gltf", "./res/positions/tree1.txt",
                                                    0.8f, 1.2f, 66);
    addObjectToScene(tree1, TREES, {.cullMethod = INSTANCED});
    ModelHandle tree1Handle{tree1.getHandle()};

    // Tree 2
    auto &tree2 = sceneResources_.loadInstancedModel(tree1Handle, "./res/models/trees/tree2/tree2.gltf",
                                                    "./res/positions/tree2.txt", 0.8f, 1.125f, 24);
    addObjectToScene(tree2, TREES, {.cullMethod = INSTANCED});

    // Tree 3
    auto &tree3 = sceneResources_.loadInstancedModel(tree1Handle, "./res/models/trees/tree3/tree3.gltf",
                                                    "./res/positions/tree3.txt", 0.85f, 1.3f, 27);
    addObjectToScene(tree3, TREES, {.cullMethod = INSTANCED});

    // Tree 4
    auto &tree4 = sceneResources_.loadInstancedModel(tree1Handle, "./res/models/trees/tree4/tree4.gltf",
                                                    "./res/positions/tree4.txt", 0.9f, 1.3f, 34);
    addObjectToScene(tree4, TREES, {.cullMethod = INSTANCED});

    // Dead tree
    auto &deadTree = sceneResources_.loadModel("./res/models/trees/dead_tree/dead_tree.gltf");
    addObjectToScene(deadTree, DIRECTIONAL_PBR, {.transform = Transform{DEAD_TREE_POS, glm::vec3{1.0f}, 45.0f}});

    /*********
     *       *
     * Rocks *
     *       *
     *********/

    // Rock 1
    auto &rock1 = sceneResources_.loadInstancedModel("./res/models/rocks/rock1/rock1.gltf", "./res/positions/rock1.txt",
                                                    0.9f, 2.0f, 11);
    addObjectToScene(rock1, DIRECTIONAL_PBR_INSTANCED, {.cullMethod = INSTANCED});
    ModelHandle rockHandle{rock1.getHandle()};

    // Rock 2
    auto &rock2 = sceneResources_.loadInstancedModel(rockHandle, "./res/models/rocks/rock2/rock2.gltf",
                                                    "./res/positions/rock2.txt", 1.45f, 2.2f, 10);
    addObjectToScene(rock2, DIRECTIONAL_PBR_INSTANCED, {.cullMethod = INSTANCED});

    // Rock 3
    auto &rock3 = sceneResources_.loadInstancedModel(rockHandle, "./res/models/rocks/rock3/rock3.gltf",
                                                    "./res/positions/rock3.txt", 0.75f, 1.0f, 14);
    addObjectToScene(rock3, DIRECTIONAL_PBR_INSTANCED, {.cullMethod = INSTANCED});

    // Rock 4
    auto &rock4 = sceneResources_.loadInstancedModel(rockHandle, "./res/models/rocks/rock4/rock4.gltf",
                                                    "./res/positions/rock4.txt", 0.8f, 1.3f, 6);
    addObjectToScene(rock4, DIRECTIONAL_PBR_INSTANCED, {.cullMethod = INSTANCED});

    // Rock 5
    auto &rock5 = sceneResources_.loadInstancedModel(rockHandle, "./res/models/rocks/rock5/rock5.gltf",
                                                    "./res/positions/rock5.txt", 0.875f, 1.1f, 6);
    addObjectToScene(rock5, DIRECTIONAL_PBR_INSTANCED, {.cullMethod = INSTANCED});

    // Rock 6
    auto &rock6 = sceneResources_.loadInstancedModel(rockHandle, "./res/models/rocks/rock6/rock6.gltf",
                                                    "./res/positions/rock6.txt", 1.2f, 1.4f, 6);
    addObjectToScene(rock6, DIRECTIONAL_PBR_INSTANCED, {.cullMethod = INSTANCED});

    // Rock 7
    auto &rock7 = sceneResources_.loadInstancedModel(rockHandle, "./res/models/rocks/rock7/rock7.gltf",
                                                    "./res/positions/rock7.txt", 0.9f, 1.125f, 8);
    addObjectToScene(rock7, DIRECTIONAL_PBR_INSTANCED, {.cullMethod = INSTANCED});

    // Rock 8
    auto &rock8 = sceneResources_.loadInstancedModel(rockHandle, "./res/models/rocks/rock8/rock8.gltf",
                                                    "./res/positions/rock8.txt", 1.0f, 1.25f, 8);
    addObjectToScene(rock8, DIRECTIONAL_PBR_INSTANCED, {.cullMethod = INSTANCED});

    /*********
     *       *
     * Water *
     *       *
     *********/

    // Main pond
    auto &pond = sceneResources_.loadModel("./res/models/water/pond/water.gltf");
    addObjectToScene(pond, WATER, {.transform = Transform{POND_WATER_POS, POND_WATER_SCALE}});

    // Small starting area puddle
    auto &puddle = sceneResources_.loadModel("./res/models/water/puddle/water.gltf");
    addObjectToScene(puddle, WATER, {.transform = Transform{PUDDLE_POS, PUDDLE_SCALE}});

    // Ocean
    auto &ocean = sceneResources_.loadModel("./res/models/water/ocean/water.gltf");
    addObjectToScene(ocean, OCEAN, {.transform = Transform{OCEAN_POS, OCEAN_SCALE}});

    /**********
     *        *
     * Skybox *
     *        *
     **********/

    std::vector<std::filesystem::path> skyBoxPaths{
        "./res/textures/skybox/right.png",  "./res/textures/skybox/left.png",  "./res/textures/skybox/top.png",
        "./res/textures/skybox/bottom.png", "./res/textures/skybox/front.png", "./res/textures/skybox/back.png"};

    auto &skybox = sceneResources_.loadCubeMappedModel("./res/models/set_pieces/cube/cube.gltf", skyBoxPaths, true);
    addObjectToScene(skybox, SKYBOX, {.cullMethod = NONE});
}

void IslandScene::addAllLightsToScene()
{
    // Normalized approximation of sun direction in skybox
    globalLightSource_ = glm::vec3{-0.36669f, -0.731219f, -0.575202f};
}

void IslandScene::setupCamera(const ConfigWindow::GraphicsSettings &graphicsSettings)
{
    camera_ = Camera{CAMERA_START_POS, -100.0f};
    camera_.setFOV(cameraDefaultValues::fov,
                  static_cast<float>(graphicsSettings.screenWidth) / static_cast<float>(graphicsSettings.screenHeight));
}

void IslandScene::onUpdate(float deltaTime)
{
    // Rotate the fancy space model
    auto &spaceModel = getObjectTransform("spaceModel");
    spaceModel.rotateAroundAxis(5.0f * deltaTime, glm::vec3{0.34f, 0.87f, 0.4f});
}
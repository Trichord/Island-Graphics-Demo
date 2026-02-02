#include "island_scene.h"

#include <filesystem>
#include <iostream>
#include <vector>

#include "config_window.h"
#include "glm/glm.hpp"
#include "object_positions.h"
#include "renderable.h"
#include "scene.h"
#include "shader_types.h"
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
    std::cout << "Loading Textures...";
    sceneResources.getTextureManager().processTextureQueue();
    std::cout << " Done." << std::endl;
}

void IslandScene::addAllModelsToScene()
{
    using enum ShaderType;
    using enum CullType;
    using enum RenderType;

    // Main island
    auto &island = sceneResources.loadTerrain("./res/models/island/island.gltf");
    addObjectToScene(island, DIRECTIONAL_PBR, {.cullType = BY_MESH});

    // Dead tree
    auto &deadTree = sceneResources.loadModel("./res/models/dead_tree/dead_tree.gltf");
    addObjectToScene(deadTree, DIRECTIONAL_PBR, {.transform = {DEAD_TREE_POS, glm::vec3{1.0f}, 45.0f}});

    std::vector<std::filesystem::path> spaceBoxPaths{
        "./res//Textures/nebula/right.png",  "./res//Textures/nebula/left.png",  "./res//Textures/nebula/top.png",
        "./res//Textures/nebula/bottom.png", "./res//Textures/nebula/front.png", "./res//Textures/nebula/back.png"};

    // Fancy spinning space model
    auto &spaceModel = sceneResources.loadCubeMappedModel("./res/models/space_model/scene.gltf", spaceBoxPaths);
    addObjectToScene(spaceModel, REFLECTIVE, {.transform = {SPACE_DODECA_POS}}, "spaceModel");

    // Skybox
    std::vector<std::filesystem::path> skyBoxPaths{
        "./res/textures/skybox/right.png",  "./res/textures/skybox/left.png",  "./res/textures/skybox/top.png",
        "./res/textures/skybox/bottom.png", "./res/textures/skybox/front.png", "./res/textures/skybox/back.png"};

    auto &skybox = sceneResources.loadCubeMappedModel("./res/models/cube/cube.gltf", skyBoxPaths);
    addObjectToScene(skybox, SKYBOX);

    // Grass
    auto &grass = sceneResources.loadInstancedModel("./res/models/grass/grass.gltf", "./res/grass.txt", 0.3f, 0.5, 372);
    addObjectToScene(grass, FLOWERS, {.cullType = INSTANCED_DISTANCE});

    // White flowers
    auto &whiteFlowers = sceneResources.loadInstancedModel("./res/models/whiteflower/whiteflower.gltf",
                                                           "./res/whiteflowers.txt", 0.9f, 1.1f, 369);
    addObjectToScene(whiteFlowers, FLOWERS, {.cullType = INSTANCED_DISTANCE});

    // Blue flowers
    auto &blueFlowers = sceneResources.loadInstancedModel(whiteFlowers, "./res/models/blueflower/blueflower.gltf",
                                                          "./res/blueflowers.txt", 0.9f, 1.1f, 181);
    addObjectToScene(blueFlowers, FLOWERS, {.cullType = INSTANCED_DISTANCE});

    // Purple flowers
    auto &purpleFlowers = sceneResources.loadInstancedModel(whiteFlowers, "./res/models/purpleflower/purpleflower.gltf",
                                                            "./res/purpleflowers.txt", 0.9f, 1.1f, 255);
    addObjectToScene(purpleFlowers, FLOWERS, {.cullType = INSTANCED_DISTANCE});

    // Dandelions
    auto &dandelions = sceneResources.loadInstancedModel(whiteFlowers, "./res/models/dandelion/dandelion.gltf",
                                                         "./res/dandelions.txt", 1.0f, 1.5f, 1494);
    addObjectToScene(dandelions, FLOWERS, {.cullType = INSTANCED_DISTANCE});

    // Strawberry flowers
    auto &strawberries = sceneResources.loadInstancedModel(whiteFlowers, "./res/models/strawberry/strawberry.gltf",
                                                           "./res/strawberries.txt", 0.8f, 1.2f, 161);
    addObjectToScene(strawberries, FLOWERS, {.cullType = INSTANCED_DISTANCE});

    // Tree 1
    auto &tree1 = sceneResources.loadInstancedModel("./res/models/tree1/tree1.gltf", "./res/tree1.txt", 0.8f, 1.2f, 67);
    addObjectToScene(tree1, TREES, {.cullType = INSTANCED});

    // Tree 2
    auto &tree2 =
        sceneResources.loadInstancedModel(tree1, "./res/models/tree2/tree2.gltf", "./res/tree2.txt", 0.8f, 1.125f, 9);
    addObjectToScene(tree2, TREES, {.cullType = INSTANCED});

    // Tree 3
    auto &tree3 =
        sceneResources.loadInstancedModel(tree1, "./res/models/tree3/tree3.gltf", "./res/tree3.txt", 0.85f, 1.3f, 29);
    addObjectToScene(tree3, TREES, {.cullType = INSTANCED});

    // Tree 4
    auto &tree4 =
        sceneResources.loadInstancedModel(tree1, "./res/models/tree4/tree4.gltf", "./res/tree4.txt", 0.9f, 1.3f, 69);
    addObjectToScene(tree4, TREES, {.cullType = INSTANCED});

    // Water
    auto &water = sceneResources.loadModel("./res/models/water/water.gltf");
    addObjectToScene(water, WATER,
                     {.transform = {POND_WATER_POS, POND_WATER_SCALE}, .renderType = WATER_PLANAR});
}

void IslandScene::addAllLightsToScene()
{
    // Normalized approximation of sun direction in skybox
    globalLightSource = glm::vec3{-0.36669f, -0.731219f, -0.575202f};
}

void IslandScene::setupCamera(const ConfigWindow::GraphicsSettings &graphicsSettings)
{
    camera.setPosition(glm::vec3{0.0f, 0.0f, 30.0f});
    camera.setAspectRatio(static_cast<float>(graphicsSettings.screenWidth) /
                          static_cast<float>(graphicsSettings.screenHeight));
}
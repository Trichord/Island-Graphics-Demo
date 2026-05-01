#include "island_scene.h"

#include <filesystem>
#include <utility>
#include <vector>

#include "aabb.h"
#include "camera.h"
#include "directional_light.h"
#include "glm/glm.hpp"
#include "graphics_settings.h"
#include "model.h"
#include "object_positions.h"
#include "scene.h"
#include "graphics_shader.h"
#include "texture_manager.h"
#include "transform.h"

#include <iostream>

IslandScene::IslandScene(TextureManager &textureManager, const GraphicsSettings &graphicsSettings)
    : Scene(textureManager)
{
    initializeScene(graphicsSettings);
}

void IslandScene::initializeScene(const GraphicsSettings &graphicsSettings)
{
    setupCamera(graphicsSettings);
    addAllModelsToScene();
    setupEnvironment();
}

void IslandScene::addAllModelsToScene()
{
    using enum ShaderType;

    /*****************
     *               *
     * Main Features *
     *               *
     *****************/

    // Main island
    auto &island = sceneResources_.loadModel("./res/models/island/island.gltf");
    addObjectToScene(island, directionalPBR);

    // Statue
    auto &statue = sceneResources_.loadModel("./res/models/set_pieces/statue/statue.gltf");
    Transform statueTransform{STATUE_POSITION, glm::vec3{1.0f}, -65.0f, glm::vec3{0.0f, 1.0f, 0.0f}};
    addObjectToScene(statue, directionalPBR, std::move(statueTransform));

    /*********
     *       *
     * Rocks *
     *       *
     *********/

    // Rock 1
    auto &rock1 = sceneResources_.loadInstancedModel("./res/models/rocks/rock1/rock1.gltf", "./res/positions/rock1.txt",
                                                     0.9f, 2.0f, 11);
    addInstancedObjectToScene(rock1, directionalPBR);
    ModelHandle rockHandle{rock1.getHandle()};

    // Rock 2
    auto &rock2 = sceneResources_.loadInstancedModel(rockHandle, "./res/models/rocks/rock2/rock2.gltf",
                                                     "./res/positions/rock2.txt", 1.45f, 2.2f, 10);
    addInstancedObjectToScene(rock2, directionalPBR);

    // Rock 3
    auto &rock3 = sceneResources_.loadInstancedModel(rockHandle, "./res/models/rocks/rock3/rock3.gltf",
                                                     "./res/positions/rock3.txt", 0.75f, 1.0f, 14);
    addInstancedObjectToScene(rock3, directionalPBR);

    // Rock 4
    auto &rock4 = sceneResources_.loadInstancedModel(rockHandle, "./res/models/rocks/rock4/rock4.gltf",
                                                     "./res/positions/rock4.txt", 0.8f, 1.3f, 6);
    addInstancedObjectToScene(rock4, directionalPBR);

    // Rock 5
    auto &rock5 = sceneResources_.loadInstancedModel(rockHandle, "./res/models/rocks/rock5/rock5.gltf",
                                                     "./res/positions/rock5.txt", 0.875f, 1.1f, 6);
    addInstancedObjectToScene(rock5, directionalPBR);

    // Rock 6
    auto &rock6 = sceneResources_.loadInstancedModel(rockHandle, "./res/models/rocks/rock6/rock6.gltf",
                                                     "./res/positions/rock6.txt", 1.2f, 1.4f, 6);
    addInstancedObjectToScene(rock6, directionalPBR);

    // Rock 7
    auto &rock7 = sceneResources_.loadInstancedModel(rockHandle, "./res/models/rocks/rock7/rock7.gltf",
                                                     "./res/positions/rock7.txt", 0.9f, 1.125f, 8);
    addInstancedObjectToScene(rock7, directionalPBR);

    // Rock 8
    auto &rock8 = sceneResources_.loadInstancedModel(rockHandle, "./res/models/rocks/rock8/rock8.gltf",
                                                     "./res/positions/rock8.txt", 1.0f, 1.25f, 8);
    addInstancedObjectToScene(rock8, directionalPBR);

    /**************
     *            *
     * Vegetation *
     *            *
     **************/

    // Grass
    auto &grassModel = sceneResources_.loadInstancedModel("./res/models/vegetation/grass/grass.gltf",
                                                          "./res/positions/grass.txt", 0.3f, 0.5, 1497);
    addInstancedObjectToScene(grassModel, grass);

    // White flowers
    auto &whiteFlowers = sceneResources_.loadInstancedModel("./res/models/vegetation/whiteflower/whiteflower.gltf",
                                                            "./res/positions/whiteflower.txt", 0.9f, 1.1f, 488);
    addInstancedObjectToScene(whiteFlowers, grass);
    ModelHandle whiteFlowerHandle{whiteFlowers.getHandle()};

    // Blue flowers
    auto &blueFlowers =
        sceneResources_.loadInstancedModel(whiteFlowerHandle, "./res/models/vegetation/blueflower/blueflower.gltf",
                                           "./res/positions/blueflower.txt", 0.9f, 1.1f, 212);
    addInstancedObjectToScene(blueFlowers, grass);

    // Purple flowers
    auto &purpleFlowers =
        sceneResources_.loadInstancedModel(whiteFlowerHandle, "./res/models/vegetation/purpleflower/purpleflower.gltf",
                                           "./res/positions/purpleflower.txt", 0.9f, 1.1f, 328);
    addInstancedObjectToScene(purpleFlowers, grass);

    // Dandelions
    auto &dandelions =
        sceneResources_.loadInstancedModel(whiteFlowerHandle, "./res/models/vegetation/dandelion/dandelion.gltf",
                                           "./res/positions/dandelion.txt", 1.0f, 1.5f, 944);
    addInstancedObjectToScene(dandelions, grass);

    // Strawberry flowers
    auto &strawberries =
        sceneResources_.loadInstancedModel(whiteFlowerHandle, "./res/models/vegetation/strawberry/strawberry.gltf",
                                           "./res/positions/strawberry.txt", 0.8f, 1.2f, 120);
    addInstancedObjectToScene(strawberries, grass);

    /*********
     *       *
     * Trees *
     *       *
     *********/

    // Tree 1
    auto &tree1 = sceneResources_.loadInstancedModel("./res/models/trees/tree1/tree1.gltf", "./res/positions/tree1.txt",
                                                     0.8f, 1.2f, 66);
    addInstancedObjectToScene(tree1, trees);
    ModelHandle tree1Handle{tree1.getHandle()};

    // Tree 2
    auto &tree2 = sceneResources_.loadInstancedModel(tree1Handle, "./res/models/trees/tree2/tree2.gltf",
                                                     "./res/positions/tree2.txt", 0.8f, 1.125f, 24);
    addInstancedObjectToScene(tree2, trees);

    // Tree 3
    auto &tree3 = sceneResources_.loadInstancedModel(tree1Handle, "./res/models/trees/tree3/tree3.gltf",
                                                     "./res/positions/tree3.txt", 0.85f, 1.3f, 27);
    addInstancedObjectToScene(tree3, trees);

    // Tree 4
    auto &tree4 = sceneResources_.loadInstancedModel(tree1Handle, "./res/models/trees/tree4/tree4.gltf",
                                                     "./res/positions/tree4.txt", 0.9f, 1.3f, 34);
    addInstancedObjectToScene(tree4, trees);

    // Dead tree
    auto &deadTree = sceneResources_.loadModel("./res/models/trees/dead_tree/dead_tree.gltf");
    Transform deadTreeTransform{DEAD_TREE_POS, glm::vec3{1.0f}, 45.0f};
    addObjectToScene(deadTree, directionalPBR, std::move(deadTreeTransform));

    /*********
     *       *
     * Water *
     *       *
     *********/

    // Main pond
    auto &pond = sceneResources_.loadModel("./res/models/water/pond/pond.gltf");

    Transform pondTransform{POND_WATER_POS, POND_WATER_SCALE};
    addObjectToScene(pond, water, std::move(pondTransform));

    // Small starting area puddle
    auto &puddle = sceneResources_.loadModel("./res/models/water/puddle/puddle.gltf");
    Transform puddleTransform{PUDDLE_POS, PUDDLE_SCALE};
    addObjectToScene(puddle, water, std::move(puddleTransform));

    // Ocean
    auto &oceanModel = sceneResources_.loadModel("./res/models/water/ocean/ocean.gltf");
    Transform oceanTransform{OCEAN_POS, OCEAN_SCALE};
    addObjectToScene(oceanModel, ocean, std::move(oceanTransform));

    /**********
     *        *
     * Skybox *
     *        *
     **********/

    std::vector<std::filesystem::path> skyBoxPaths{
        "./res/textures/skybox/right.png",  "./res/textures/skybox/left.png",  "./res/textures/skybox/top.png",
        "./res/textures/skybox/bottom.png", "./res/textures/skybox/front.png", "./res/textures/skybox/back.png"};

    auto &skyboxModel =
        sceneResources_.loadCubeMappedModel("./res/models/set_pieces/skybox/skybox.gltf", skyBoxPaths, false);
    addObjectToScene(skyboxModel, skybox);

    /***************
     *             *
     * Space Model *
     *             *
     ***************/

    // Fancy spinning space model
    std::vector<std::filesystem::path> spaceBoxPaths{
        "./res/textures/nebula/right.png",  "./res/textures/nebula/left.png",  "./res/textures/nebula/top.png",
        "./res/textures/nebula/bottom.png", "./res/textures/nebula/front.png", "./res/textures/nebula/back.png"};

    auto &spaceModel =
        sceneResources_.loadCubeMappedModel("./res/models/set_pieces/space_model/space_model.gltf", spaceBoxPaths);
    Transform spaceDodecaTransform{SPACE_DODECA_POS};

    addObjectToScene(spaceModel, reflective, std::move(spaceDodecaTransform), "spaceModel");
}

void IslandScene::setupEnvironment()
{
    // Bright midday sun
    constexpr glm::vec3 lightColor{1.0f, 0.95f, 0.9f};

    // Normalized approximation of sun direction in skybox
    constexpr glm::vec3 sunDirection{-0.36669f, -0.731219f, -0.575202f};

    // Slightly darker ambient
    constexpr glm::vec3 dampenedAmbient{0.168754f};

    ambientLight_ = dampenedAmbient;
    globalLightSource_ = DirectionalLight{glm::vec3{sunDirection}, glm::vec3{lightColor}};

    // Normalized wind direction, east southeast with a downward angle
    constexpr glm::vec3 windDir{0.858185f, -0.289923f, 0.423631f};
    constexpr float windStrength{0.7f};

    windDirection_ = windDir;
    windStrength_ = windStrength;
}

void IslandScene::setupCamera(const GraphicsSettings &graphicsSettings)
{
    constexpr float cameraFOV{45.0f};

    camera_ = Camera{CAMERA_START_POS, -100.0f};
    camera_.setFOV(cameraFOV);
    camera_.setAspectRatio(static_cast<float>(graphicsSettings.screenWidth) /
                           static_cast<float>(graphicsSettings.screenHeight));
}

void IslandScene::onUpdate(float deltaTime)
{
    // Rotate the fancy space model
    auto &spaceModelEntry = getEntryByName("spaceModel");
    Model &spaceModel{spaceModelEntry.modelRef.get()};
    AABB &spaceAABB{spaceModel.getAABB()};

    for (std::size_t i{0}; i < spaceModelEntry.transforms.size(); ++i)
        spaceModelEntry.transforms[i].rotateAroundAxis(deltaTime, glm::vec3{0.34f, 0.935f, 0.6f});

    updatedModels_.insert(spaceModel.getHandle());
}
#include "island_scene.h"

#include <filesystem>
#include <utility>
#include <vector>

#include "aabb.h"
#include "camera.h"
#include "directional_light.h"
#include "glm/glm.hpp"
#include "graphics_settings.h"
#include "graphics_shader.h"
#include "object_positions.h"
#include "renderable.h"
#include "scene.h"
#include "texture_manager.h"
#include "transform.h"

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
    auto &island = resources_.loadModel("./res/models/island/island.gltf");
    addObjectToScene(island, directionalPBR);

    // Statue
    auto &statue = resources_.loadModel("./res/models/set_pieces/statue/statue.gltf");
    Transform statueTransform{STATUE_POSITION, glm::vec3{1.0f}, -65.0f, glm::vec3{0.0f, 1.0f, 0.0f}};
    addObjectToScene(statue, directionalPBR, std::move(statueTransform));

    /*********
     *       *
     * Rocks *
     *       *
     *********/

    // Rock 1
    auto &rock1 = resources_.loadInstancedModel("./res/models/rocks/rock1/rock1.gltf", "./res/positions/rock1.txt",
                                                {.minScale = 0.9f, .maxScale = 2.0f});
    addInstancedObjectToScene(rock1, directionalPBR);
    Handle rockHandle{rock1.getHandle()};

    // Rock 2
    auto &rock2 = resources_.loadInstancedModel("./res/models/rocks/rock2/rock2.gltf", "./res/positions/rock2.txt",
                                                {.minScale = 1.45f, .maxScale = 2.2f}, rockHandle);
    addInstancedObjectToScene(rock2, directionalPBR);

    // Rock 3
    auto &rock3 = resources_.loadInstancedModel("./res/models/rocks/rock3/rock3.gltf", "./res/positions/rock3.txt",
                                                {.minScale = 0.75f, .maxScale = 1.0f}, rockHandle);
    addInstancedObjectToScene(rock3, directionalPBR);

    // Rock 4
    auto &rock4 = resources_.loadInstancedModel("./res/models/rocks/rock4/rock4.gltf", "./res/positions/rock4.txt",
                                                {.minScale = 0.8f, .maxScale = 1.3f}, rockHandle);
    addInstancedObjectToScene(rock4, directionalPBR);

    // Rock 5
    auto &rock5 = resources_.loadInstancedModel("./res/models/rocks/rock5/rock5.gltf", "./res/positions/rock5.txt",
                                                {.minScale = 0.875f, .maxScale = 1.1f}, rockHandle);
    addInstancedObjectToScene(rock5, directionalPBR);

    // Rock 6
    auto &rock6 = resources_.loadInstancedModel("./res/models/rocks/rock6/rock6.gltf", "./res/positions/rock6.txt",
                                                {.minScale = 1.2f, .maxScale = 1.4f}, rockHandle);
    addInstancedObjectToScene(rock6, directionalPBR);

    // Rock 7
    auto &rock7 = resources_.loadInstancedModel("./res/models/rocks/rock7/rock7.gltf", "./res/positions/rock7.txt",
                                                {.minScale = 0.9f, .maxScale = 1.125f}, rockHandle);
    addInstancedObjectToScene(rock7, directionalPBR);

    // Rock 8
    auto &rock8 = resources_.loadInstancedModel("./res/models/rocks/rock8/rock8.gltf", "./res/positions/rock8.txt",
                                                {.minScale = 1.0f, .maxScale = 1.25f}, rockHandle);
    addInstancedObjectToScene(rock8, directionalPBR);

    /**************
     *            *
     * Vegetation *
     *            *
     **************/

    // Grass0.3f, 0.5
    auto &grassModel = resources_.loadInstancedModel("./res/models/vegetation/grass/grass.gltf",
                                                     "./res/positions/grass.txt", {.minScale = 0.3f, .maxScale = 0.5f});
    addInstancedObjectToScene(grassModel, grass);

    // White flowers
    auto &whiteFlowers =
        resources_.loadInstancedModel("./res/models/vegetation/whiteflower/whiteflower.gltf",
                                      "./res/positions/whiteflower.txt", {.minScale = 0.9f, .maxScale = 1.1f});
    addInstancedObjectToScene(whiteFlowers, grass);
    Handle whiteFlowerHandle{whiteFlowers.getHandle()};

    // Blue flowers
    auto &blueFlowers = resources_.loadInstancedModel("./res/models/vegetation/blueflower/blueflower.gltf",
                                                      "./res/positions/blueflower.txt",
                                                      {.minScale = 0.9f, .maxScale = 1.1f}, whiteFlowerHandle);
    addInstancedObjectToScene(blueFlowers, grass);

    // Purple flowers
    auto &purpleFlowers = resources_.loadInstancedModel("./res/models/vegetation/purpleflower/purpleflower.gltf",
                                                        "./res/positions/purpleflower.txt",
                                                        {.minScale = 0.9f, .maxScale = 1.1f}, whiteFlowerHandle);
    addInstancedObjectToScene(purpleFlowers, grass);

    // Dandelions
    auto &dandelions = resources_.loadInstancedModel("./res/models/vegetation/dandelion/dandelion.gltf",
                                                     "./res/positions/dandelion.txt",
                                                     {.minScale = 1.0f, .maxScale = 1.5f}, whiteFlowerHandle);
    addInstancedObjectToScene(dandelions, grass);

    // Strawberry flowers
    auto &strawberries = resources_.loadInstancedModel("./res/models/vegetation/strawberry/strawberry.gltf",
                                                       "./res/positions/strawberry.txt",
                                                       {.minScale = 0.8f, .maxScale = 1.2f}, whiteFlowerHandle);
    addInstancedObjectToScene(strawberries, grass);

    /*********
     *       *
     * Trees *
     *       *
     *********/

    // Tree 1
    auto &tree1 = resources_.loadInstancedModel("./res/models/trees/tree1/tree1.gltf", "./res/positions/tree1.txt",
                                                {.minScale = 0.8f, .maxScale = 1.2f});
    addInstancedObjectToScene(tree1, trees);
    Handle tree1Handle{tree1.getHandle()};

    // Tree 2
    auto &tree2 = resources_.loadInstancedModel("./res/models/trees/tree2/tree2.gltf", "./res/positions/tree2.txt",
                                                {.minScale = 0.8f, .maxScale = 1.125f}, tree1Handle);
    addInstancedObjectToScene(tree2, trees);

    // Tree 3
    auto &tree3 = resources_.loadInstancedModel("./res/models/trees/tree3/tree3.gltf", "./res/positions/tree3.txt",
                                                {.minScale = 0.85f, .maxScale = 1.3f}, tree1Handle);
    addInstancedObjectToScene(tree3, trees);

    // Tree 4
    auto &tree4 = resources_.loadInstancedModel("./res/models/trees/tree4/tree4.gltf", "./res/positions/tree4.txt",
                                                {.minScale = 0.9f, .maxScale = 1.3f}, tree1Handle);
    addInstancedObjectToScene(tree4, trees);

    // Dead tree
    auto &deadTree = resources_.loadModel("./res/models/trees/dead_tree/dead_tree.gltf");
    Transform deadTreeTransform{DEAD_TREE_POS, glm::vec3{1.0f}, 45.0f};
    addObjectToScene(deadTree, directionalPBR, std::move(deadTreeTransform));

    /*********
     *       *
     * Water *
     *       *
     *********/

    // Main pond
    auto &pond = resources_.loadModel("./res/models/water/pond/pond.gltf");

    Transform pondTransform{POND_WATER_POS, POND_WATER_SCALE};
    addObjectToScene(pond, water, std::move(pondTransform));

    // Small starting area puddle
    auto &puddle = resources_.loadModel("./res/models/water/puddle/puddle.gltf");
    Transform puddleTransform{PUDDLE_POS, PUDDLE_SCALE};
    addObjectToScene(puddle, water, std::move(puddleTransform));

    // Ocean
    auto &oceanModel = resources_.loadModel("./res/models/water/ocean/ocean.gltf");
    Transform oceanTransform{OCEAN_POS, OCEAN_SCALE};
    addObjectToScene(oceanModel, ocean, std::move(oceanTransform));

    /**********
     *        *
     * Skybox *
     *        *
     **********/

    std::filesystem::path skyBoxPath{"./res/textures/skybox/skybox.dds"};

    auto &skyboxModel = resources_.loadSkybox(skyBoxPath);
    addSkyboxToScene(skyboxModel);

    /***************
     *             *
     * Space Model *
     *             *
     ***************/

    // Fancy spinning space model
    std::filesystem::path spaceBoxPath{"./res/textures/nebula/nebula.dds"};

    auto &spaceModel =
        resources_.loadCubeMappedModel("./res/models/set_pieces/space_model/space_model.gltf", spaceBoxPath);
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

    // Normalized wind direction, east southeast with a downward
    // angle
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
    Renderable &spaceModel{spaceModelEntry.renderableRef.get()};
    AABB &spaceAABB{spaceModel.getAABB()};

    for (std::size_t i{0}; i < spaceModelEntry.transforms.size(); ++i)
        spaceModelEntry.transforms[i].rotateAroundAxis(deltaTime, glm::vec3{0.34f, 0.935f, 0.6f});

    updatedModels_.insert(spaceModel.getHandle());
}
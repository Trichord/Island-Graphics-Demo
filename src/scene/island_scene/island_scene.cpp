#include "island_scene.h"

#include <filesystem>
#include <utility>
#include <vector>

#include "camera.h"
#include "directional_light.h"
#include "glm/glm.hpp"
#include "graphics_settings.h"
#include "graphics_shader.h"
#include "instanced_model.h"
#include "model.h"
#include "object_positions.h"
#include "scene.h"
#include "skybox.h"
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

    Model island{"./res/models/island/island.gltf"};
    addModel(island, {}, directionalPBR);

    Model statue{"./res/models/set_pieces/statue/statue.gltf"};
    Transform statueTransform{STATUE_POSITION, glm::vec3{1.0f}, -65.0f, glm::vec3{0.0f, 1.0f, 0.0f}};
    addModel(statue, std::move(statueTransform), directionalPBR);

    // Fancy spinning space model
    std::filesystem::path spaceSkybox{"./res/textures/nebula/nebula.ktx2"};
    Model spaceModel{"./res/models/set_pieces/space_model/space_model.gltf"};
    Transform spaceDodecaTransform{SPACE_DODECA_POS};
    addCubemappedModel(spaceModel, spaceSkybox, std::move(spaceDodecaTransform), reflective, "spaceModel");

    /*********
     *       *
     * Rocks *
     *       *
     *********/

    InstancedModel rock1{
        "./res/models/rocks/rock1/rock1.gltf", "./res/positions/rock1.txt", {.minScale = 0.9f, .maxScale = 2.0f}};
    addInstancedModel(rock1, directionalPBR);

    InstancedModel rock2{
        "./res/models/rocks/rock2/rock2.gltf", "./res/positions/rock2.txt", {.minScale = 1.45f, .maxScale = 2.2f}};
    addInstancedModel(rock2, directionalPBR);

    InstancedModel rock3{
        "./res/models/rocks/rock3/rock3.gltf", "./res/positions/rock3.txt", {.minScale = 0.75f, .maxScale = 1.0f}};
    addInstancedModel(rock3, directionalPBR);

    InstancedModel rock4{
        "./res/models/rocks/rock4/rock4.gltf", "./res/positions/rock4.txt", {.minScale = 0.8f, .maxScale = 1.3f}};
    addInstancedModel(rock4, directionalPBR);

    InstancedModel rock5{
        "./res/models/rocks/rock5/rock5.gltf", "./res/positions/rock5.txt", {.minScale = 0.875f, .maxScale = 1.1f}};
    addInstancedModel(rock5, directionalPBR);

    InstancedModel rock6{
        "./res/models/rocks/rock6/rock6.gltf", "./res/positions/rock6.txt", {.minScale = 1.2f, .maxScale = 1.4f}};
    addInstancedModel(rock6, directionalPBR);

    InstancedModel rock7{
        "./res/models/rocks/rock7/rock7.gltf", "./res/positions/rock7.txt", {.minScale = 0.9f, .maxScale = 1.125f}};
    addInstancedModel(rock7, directionalPBR);

    InstancedModel rock8{
        "./res/models/rocks/rock8/rock8.gltf", "./res/positions/rock8.txt", {.minScale = 1.0f, .maxScale = 1.25f}};
    addInstancedModel(rock8, directionalPBR);

    /**************
     *            *
     * Vegetation *
     *            *
     **************/

    InstancedModel grassModel{
        "./res/models/vegetation/grass/grass.gltf", "./res/positions/grass.txt", {.minScale = 0.3f, .maxScale = 0.5f}};
    addInstancedModel(grassModel, grass);

    InstancedModel whiteFlowers{"./res/models/vegetation/whiteflower/whiteflower.gltf",
                                "./res/positions/whiteflower.txt",
                                {.minScale = 0.9f, .maxScale = 1.1f}};
    addInstancedModel(whiteFlowers, grass);

    InstancedModel blueFlowers{"./res/models/vegetation/blueflower/blueflower.gltf",
                               "./res/positions/blueflower.txt",
                               {.minScale = 0.9f, .maxScale = 1.1f}};
    addInstancedModel(blueFlowers, grass);

    InstancedModel purpleFlowers{"./res/models/vegetation/purpleflower/purpleflower.gltf",
                                 "./res/positions/purpleflower.txt",
                                 {.minScale = 0.9f, .maxScale = 1.1f}};
    addInstancedModel(purpleFlowers, grass);

    InstancedModel dandelions{"./res/models/vegetation/dandelion/dandelion.gltf",
                              "./res/positions/dandelion.txt",
                              {.minScale = 1.0f, .maxScale = 1.5f}};
    addInstancedModel(dandelions, grass);

    InstancedModel strawberries{"./res/models/vegetation/strawberry/strawberry.gltf",
                                "./res/positions/strawberry.txt",
                                {.minScale = 0.8f, .maxScale = 1.2f}};
    addInstancedModel(strawberries, grass);

    /*********
     *       *
     * Trees *
     *       *
     *********/

    InstancedModel tree1{
        "./res/models/trees/tree1/tree1.gltf", "./res/positions/tree1.txt", {.minScale = 0.8f, .maxScale = 1.2f}};
    addInstancedModel(tree1, trees);

    InstancedModel tree2{
        "./res/models/trees/tree2/tree2.gltf", "./res/positions/tree2.txt", {.minScale = 0.8f, .maxScale = 1.125f}};
    addInstancedModel(tree2, trees);

    InstancedModel tree3{
        "./res/models/trees/tree3/tree3.gltf", "./res/positions/tree3.txt", {.minScale = 0.85f, .maxScale = 1.3f}};
    addInstancedModel(tree3, trees);

    InstancedModel tree4{
        "./res/models/trees/tree4/tree4.gltf", "./res/positions/tree4.txt", {.minScale = 0.9f, .maxScale = 1.3f}};
    addInstancedModel(tree4, trees);

    Model deadTree{"./res/models/trees/dead_tree/dead_tree.gltf"};
    Transform deadTreeTransform{DEAD_TREE_POS, glm::vec3{1.0f}, 45.0f};
    addModel(deadTree, std::move(deadTreeTransform), directionalPBR);

    /*********
     *       *
     * Water *
     *       *
     *********/

    // Main pond
    Model pond{"./res/models/water/pond/pond.gltf"};
    Transform pondTransform{POND_WATER_POS, POND_WATER_SCALE};
    addModel(pond, std::move(pondTransform), water);

    // Small starting area puddle
    Model puddle{"./res/models/water/puddle/puddle.gltf"};
    Transform puddleTransform{PUDDLE_POS, PUDDLE_SCALE};
    addModel(puddle, std::move(puddleTransform), water);

    // Ocean
    Model oceanModel{"./res/models/water/ocean/ocean.gltf"};
    Transform oceanTransform{OCEAN_POS, OCEAN_SCALE};
    addModel(oceanModel, std::move(oceanTransform), ocean);

    /**********
     *        *
     * Skybox *
     *        *
     **********/

    std::filesystem::path skyBoxPath{"./res/textures/skybox/skybox.ktx2"};
    Skybox skyboxModel{skyBoxPath};
    addSkybox(skyboxModel);
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
    SceneEntry &spaceModel{getEntryByName("spaceModel")};

    for (std::size_t i{0}; i < spaceModel.transforms.size(); ++i)
        spaceModel.transforms[i].rotateAroundAxis(deltaTime, glm::vec3{0.34f, 0.935f, 0.6f});

    updatedModels_.insert(spaceModel.handle);
}
#include "scene.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "camera.h"
#include "directional_light.h"
#include "glm/glm.hpp"
#include "graphics_settings.h"
#include "graphics_shader.h"
#include "handle.h"
#include "instanced_model.h"
#include "model.h"
#include "scene_file.h"
#include "skybox.h"
#include "texture_manager.h"
#include "transform.h"
#include "json/json.hpp"

Scene::Scene(TextureManager &textureManager, const GraphicsSettings &graphicsSettings,
             const std::filesystem::path &jsonFile)
    : textureManager_{textureManager}
{
    setupScene(jsonFile, graphicsSettings);
}

std::vector<Scene::Entry> &Scene::getAllSceneEntries()
{
    return entries_;
}

Scene::Entry &Scene::getEntryByName(const std::string &key)
{
    auto iter = entriesByName_.find(key);

    if (iter == entriesByName_.end() || iter->second >= entries_.size())
    {
        std::cerr << "ERROR: Cannot find named scene entry " << key << std::endl;
        throw std::runtime_error("Invalid key");
    }

    return entries_[iter->second];
}

DirectionalLight &Scene::getGlobalLight()
{
    return globalLightSource_;
}

glm::vec3 Scene::getAmbientLight() const
{
    return ambientLight_;
}

void Scene::setAmbientLight(const glm::vec3 &ambient)
{
    ambientLight_ = ambient;
}

Camera &Scene::getCamera()
{
    return camera_;
}

const glm::vec3 &Scene::getWindDirection() const
{
    return windDirection_;
}

float Scene::getWindStrength() const
{
    return windStrength_;
}

std::unordered_set<Handle> &Scene::getUpdatedModels()
{
    return updatedModels_;
}

Handle Scene::loadGLTFTextures(const Model &model)
{
    const Handle handle{textureManager_.assignHandle()};
    const std::filesystem::path modelPath{model.getPath()};
    textureManager_.loadModelTextures(handle, model.getAsset(), modelPath.parent_path());

    return handle;
}

void Scene::addSkybox(const Skybox &skybox)
{
    const Handle handle{textureManager_.assignHandle()};
    const std::filesystem::path image{skybox.getPath()};

    textureManager_.loadCubemapTextures(handle, image);

    entries_.emplace_back(Entry{
        .handle = handle,
        .aabb = skybox.getAABB(),
        .primitives = skybox.getPrimitives(),
        .transforms = {{}},
        .shaderType = ShaderType::skybox,
    });
}

void Scene::addModel(const Model &model, const Transform &transform, ShaderType shaderType, const std::string &name)
{
    const Handle handle{loadGLTFTextures(model)};

    if (!name.empty())
        entriesByName_.emplace(name, entries_.size());

    entries_.emplace_back(Entry{
        .handle = handle,
        .aabb = model.getAABB(),
        .primitives = model.getPrimitives(),
        .transforms = {transform},
        .filePath = model.getPath(),
        .shaderType = shaderType,
    });
}

void Scene::addCubemappedModel(const Model &model, const std::filesystem::path &imagePath, const Transform &transform,
                               ShaderType shaderType, const std::string &name)
{
    const Handle handle{loadGLTFTextures(model)};
    textureManager_.loadCubemapTextures(handle, imagePath);

    if (!name.empty())
        entriesByName_.emplace(name, entries_.size());

    entries_.emplace_back(Entry{
        .handle = handle,
        .aabb = model.getAABB(),
        .primitives = model.getPrimitives(),
        .transforms = {transform},
        .filePath = model.getPath(),
        .shaderType = shaderType,
    });
}

void Scene::addInstancedModel(const InstancedModel &model, ShaderType shaderType, const std::string &name)
{
    const Handle handle{loadGLTFTextures(model)};

    if (!name.empty())
        entriesByName_.emplace(name, entries_.size());

    std::vector<Transform> transforms{model.loadInstancedTransforms()};

    entries_.emplace_back(Entry{
        .handle = handle,
        .aabb = model.getAABB(),
        .primitives = model.getPrimitives(),
        .transforms = std::move(transforms),
        .filePath = model.getPath(),
        .shaderType = shaderType,
    });
}

// Helper for the visitor pattern
template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};

void Scene::setupScene(const std::filesystem::path &jsonFile, const GraphicsSettings &graphicsSettings)
{
    using namespace nlohmann;

    if (jsonFile.empty() || !std::filesystem::exists(jsonFile))
    {
        std::cerr << "ERROR: Could not load scene " << jsonFile << std::endl;
        throw std::runtime_error("Invalid Scene");
    }

    std::ifstream file{jsonFile};

    if (!file.is_open())
    {
        std::cerr << "ERROR: Failed to open " << jsonFile << std::endl;
        throw std::runtime_error("Failed to open file");
    }
    SceneDescription scene;
    
    try
    {
    scene = json::parse(file).get<SceneDescription>();
    }
    catch (const json::exception &e)
    {
        std::cerr << e.what() << std::endl;
        throw;
    }

    camera_ = Camera{scene.camera.position, scene.camera.yaw, scene.camera.pitch};
    camera_.setFOV(scene.camera.FOV);
    camera_.setNearPlane(scene.camera.nearPlane);
    camera_.setFarPlane(scene.camera.farPlane);
    camera_.setAspectRatio(graphicsSettings.screenWidth / static_cast<float>(graphicsSettings.screenHeight));

    ambientLight_ = scene.environment.ambientLight;
    globalLightSource_ = DirectionalLight{scene.environment.sunDirection, scene.environment.lightColor};

    windStrength_ = scene.wind.strength;
    windDirection_ = scene.wind.direction;

    for (auto &sceneEntry : scene.entries)
    {
        std::visit(overloaded{
                       [&](const ModelEntry &entry) {
                           Model model{entry.modelPath};
                           addModel(model, entry.transform, entry.shader, entry.name);
                       },
                       [&](const InstancedModelEntry &entry) {
                           InstancedModel model{
                               entry.modelPath, entry.positionsPath,
                               InstancedModel::Randomize{.minScale = entry.minScale, .maxScale = entry.maxScale}};
                           addInstancedModel(model, entry.shader, entry.name);
                       },
                       [&](const CubeMappedModelEntry &entry) {
                           Model model{entry.modelPath};
                           addCubemappedModel(model, entry.cubemapPath, entry.transform, entry.shader, entry.name);
                       },
                       [&](const SkyboxEntry &entry) {
                           Skybox skybox{entry.skyboxPath};
                           addSkybox(skybox);
                       },
                   },
                   sceneEntry);
    }
}
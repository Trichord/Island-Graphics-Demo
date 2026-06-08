#include "scene/scene.h"

#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "application/graphics_settings.h"
#include "models/instanced_model.h"
#include "models/model.h"
#include "models/skybox.h"
#include "renderer/shader/graphics_shader.h"
#include "scene/camera.h"
#include "scene/directional_light.h"
#include "scene/handle.h"
#include "scene/scene_file.h"
#include "utilities/texture_manager.h"
#include "utilities/transform.h"

namespace
{
void validateSceneEntry(Scene::Entry &entry)
{
    using enum ShaderType;
    using enum fastgltf::AlphaMode;

    if (entry.shaderType == ocean)
    {
        if (entry.primitives.front().alphaMode == Opaque)
            return;

        for (Primitive &primitive : entry.primitives)
        {
            if (primitive.alphaMode == Opaque)
                continue;

            std::cerr << "WARNING: Overriding ocean shader primitive alpha mode wtih Opaque" << std::endl;
            primitive.alphaMode = Opaque;
        }
    }

    if (entry.shaderType == water)
    {
        if (entry.primitives.front().alphaMode == Blend)
            return;

        for (Primitive &primitive : entry.primitives)
        {
            if (primitive.alphaMode == Blend)
                continue;

            std::cerr << "WARNING: Overriding water shader primitive alpha mode wtih Blend" << std::endl;
            primitive.alphaMode = Blend;
        }
    }
}
} // namespace

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

Scene::Entry &Scene::getEntryByHandle(Handle handle)
{
    auto iter = entriesByHandle_.find(handle);

    if (iter == entriesByHandle_.end() || iter->second >= entries_.size())
    {
        std::cerr << "ERROR: Cannot find scene entry with handle " << handle << std::endl;
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

void Scene::loadGLTFTextures(Handle handle, const Model &model)
{
    const std::filesystem::path modelPath{model.getPath()};

    if (model.getAsset().materials.empty())
        textureManager_.addDefaultMaterial(handle);
    else
        textureManager_.loadModelTextures(handle, model.getAsset(), modelPath.parent_path());
}

void Scene::addParticles(Handle handle, const Particles &particles)
{
    textureManager_.addDefaultMaterial(handle);

    entries_.emplace_back(Entry{
        .handle = handle,
        .aabb = particles.getAABB(),
        .primitives = particles.getPrimitives(),
        .transforms = {{}},
        .shaderType = ShaderType::particles,
    });
}

void Scene::addSkybox(Handle handle, const Skybox &skybox)
{
    const std::filesystem::path image{skybox.getPath()};

    textureManager_.addDefaultMaterial(handle);
    textureManager_.loadCubemapTextures(handle, image);

    entries_.emplace_back(Entry{
        .handle = handle,
        .aabb = skybox.getAABB(),
        .primitives = skybox.getPrimitives(),
        .transforms = {{}},
        .shaderType = ShaderType::skybox,
    });
}

void Scene::addModel(Handle handle, const Model &model, const Transform &transform, ShaderType shaderType,
                     const std::string &name)
{
    if (!name.empty())
        entriesByName_.emplace(name, entries_.size());

    loadGLTFTextures(handle, model);

    entries_.emplace_back(Entry{
        .handle = handle,
        .aabb = model.getAABB(),
        .primitives = model.getPrimitives(),
        .transforms = {transform},
        .filePath = model.getPath(),
        .shaderType = shaderType,
    });
}

void Scene::addCubemappedModel(Handle handle, const Model &model, const std::filesystem::path &imagePath,
                               const Transform &transform, ShaderType shaderType, const std::string &name)
{
    if (!name.empty())
        entriesByName_.emplace(name, entries_.size());

    loadGLTFTextures(handle, model);
    textureManager_.loadCubemapTextures(handle, imagePath);

    entries_.emplace_back(Entry{
        .handle = handle,
        .aabb = model.getAABB(),
        .primitives = model.getPrimitives(),
        .transforms = {transform},
        .filePath = model.getPath(),
        .shaderType = shaderType,
    });
}

void Scene::addInstancedModel(Handle handle, const InstancedModel &model, ShaderType shaderType,
                              const std::string &name)
{
    if (!name.empty())
        entriesByName_.emplace(name, entries_.size());

    loadGLTFTextures(handle, model);

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
        // Names are optional, entries by handle is not
        std::size_t entryIndex{entries_.size()};
        Handle handle{textureManager_.assignHandle()};

        std::visit(overloaded{
                       [&](const ParticlesEntry &entry) {
                           Particles particles{entry.translation,    entry.extent,   entry.color,
                                               entry.colorVariation, entry.lifetime, entry.count};
                           addParticles(handle, particles);
                       },
                       [&](const SkyboxEntry &entry) {
                           Skybox skybox{entry.skyboxPath};
                           addSkybox(handle, skybox);
                       },
                       [&](const ModelEntry &entry) {
                           Model model{entry.modelPath};
                           addModel(handle, model, entry.transform, entry.shader, entry.name);
                       },
                       [&](const InstancedModelEntry &entry) {
                           InstancedModel model{
                               entry.modelPath, entry.positionsPath,
                               InstancedModel::Randomize{.minScale = entry.minScale, .maxScale = entry.maxScale}};
                           addInstancedModel(handle, model, entry.shader, entry.name);
                       },
                       [&](const CubeMappedModelEntry &entry) {
                           Model model{entry.modelPath};
                           addCubemappedModel(handle, model, entry.cubemapPath, entry.transform, entry.shader,
                                              entry.name);
                       },
                   },
                   sceneEntry);

        validateSceneEntry(entries_.back());
        entriesByHandle_[handle] = entryIndex;
    }
}
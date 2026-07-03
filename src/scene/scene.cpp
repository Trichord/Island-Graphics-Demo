#include "scene/scene.h"

#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "application/graphics_settings.h"
#include "models/animation/animator.h"
#include "models/instanced_model.h"
#include "models/model.h"
#include "models/particles.h"
#include "models/primitive.h"
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

// Helper for the visitor pattern
template <class... Ts> struct overloaded : Ts...
{
    using Ts::operator()...;
};

void validateSceneEntry(Scene::Entry &entry)
{
    using enum ShaderType;
    using enum AlphaMode;

    if (entry.shaderType == ocean)
    {
        if (entry.primitives.front().alphaMode == opaque)
            return;

        for (Primitive &primitive : entry.primitives)
        {
            if (primitive.alphaMode == opaque)
                continue;

            std::cerr << "WARNING: Overriding ocean shader primitive alpha mode with Opaque" << std::endl;
            primitive.alphaMode = opaque;
        }
    }

    if (entry.shaderType == water)
    {
        if (entry.primitives.front().alphaMode == blend)
            return;

        for (Primitive &primitive : entry.primitives)
        {
            if (primitive.alphaMode == blend)
                continue;

            std::cerr << "WARNING: Overriding water shader primitive alpha mode with Blend" << std::endl;
            primitive.alphaMode = blend;
        }
    }
}

std::optional<Scene::EntryAnimation> createEntryAnimation(const Model &model)
{
    const auto &skeletalAnimation = model.getSkeletalAnimation();

    if (!skeletalAnimation.has_value())
        return {};

    auto &skin = skeletalAnimation->skeleton.skin;

    std::unordered_map<std::string, std::size_t> map;

    for (std::size_t i{0}; i < skeletalAnimation->animations.size(); ++i)
    {
        const auto &animation = skeletalAnimation->animations[i];

        if (!animation.name.empty())
            map.emplace(animation.name, i);
    }

    std::size_t numJoints{skin.has_value() ? skin.value().invBindMatrices.size() : 0};

    return Scene::EntryAnimation{skeletalAnimation.value(),
                                 Animator{skeletalAnimation->skeleton.parents.size(), numJoints}, map};
}

} // namespace

Scene::Scene(TextureManager &textureManager, const GraphicsSettings &graphicsSettings,
             const std::filesystem::path &jsonFile)
    : textureManager_{textureManager}
{
    setupScene(jsonFile, graphicsSettings);
}

void Scene::updateHandle(Handle handle)
{
    updatedModels_.insert(handle);
}

const std::vector<Scene::Entry> &Scene::getAllSceneEntries() const
{
    return entries_;
}

std::size_t Scene::findHandle(Handle handle) const
{
    auto iter = entriesByHandle_.find(handle);

    if (iter == entriesByHandle_.end() || iter->second >= entries_.size())
    {
        std::cerr << "ERROR: Cannot find scene entry with handle " << handle << std::endl;
        throw std::runtime_error("Invalid key");
    }

    return iter->second;
}

std::size_t Scene::findName(const std::string &name) const
{
    auto iter = entriesByName_.find(name);

    if (iter == entriesByName_.end() || iter->second >= entries_.size())
    {
        std::cerr << "ERROR: Cannot find named scene entry " << name << std::endl;
        throw std::runtime_error("Invalid key");
    }

    return iter->second;
}

const Scene::Entry &Scene::getEntryByName(const std::string &name) const
{
    return entries_[findName(name)];
}

const Scene::Entry &Scene::getEntryByHandle(Handle handle) const
{
    return entries_[findHandle(handle)];
}

Scene::Entry &Scene::getEntry(const std::string &name)
{
    return entries_[findName(name)];
}

Scene::Entry &Scene::getEntry(const Handle handle)
{
    return entries_[findHandle(handle)];
}

DirectionalLight &Scene::getGlobalLight()
{
    return globalLightSource_;
}

const glm::vec3 &Scene::getAmbientLight() const
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

std::unordered_set<Handle> &Scene::getUpdatedHandles()
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
        .shaderType = ShaderType::particles,
        .primitives = particles.getPrimitives(),
        .worldTransforms = {{}},
    });
}

void Scene::addSkybox(Handle handle, const Skybox &skybox)
{
    const std::filesystem::path image{skybox.getPath()};

    textureManager_.addDefaultMaterial(handle);
    textureManager_.loadCubemapTextures(handle, image);

    entries_.emplace_back(Entry{
        .handle = handle,
        .shaderType = ShaderType::skybox,
        .primitives = skybox.getPrimitives(),
        .worldTransforms = {{}},
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
        .shaderType = shaderType,
        .primitives = model.getPrimitives(),
        .worldTransforms = {transform},
        .entryAnimation = createEntryAnimation(model),
        .modelPath = model.getPath(),
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
        .shaderType = shaderType,
        .primitives = model.getPrimitives(),
        .worldTransforms = {transform},
        .entryAnimation = createEntryAnimation(model),
        .modelPath = model.getPath(),
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
        .shaderType = shaderType,
        .primitives = model.getPrimitives(),
        .worldTransforms = std::move(transforms),
        .entryAnimation = createEntryAnimation(model),
        .modelPath = model.getPath(),
    });
}

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
        Handle handle{nextHandle_++};

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

void Scene::playAnimation(Scene::Entry &entry, const std::string &name, float deltaTime)
{
    if (!entry.entryAnimation.has_value())
        return;

    std::unordered_map<std::string, std::size_t> &map{entry.entryAnimation->animationMap};

    auto nameIter = map.find(name);

    if (nameIter == map.end())
    {
        std::cerr << "WARNING: Animation " << name << " not found" << std::endl;
        return;
    }

    Scene::EntryAnimation &entryAnimation{entry.entryAnimation.value()};
    const Skeleton &skeleton{entryAnimation.skeletalAnimation.skeleton};

    entryAnimation.animator.play(entryAnimation.skeletalAnimation.animations[nameIter->second], skeleton,
                                 entry.primitives, deltaTime);
}

void Scene::playAnimation(Scene::Entry &entry, int index, float deltaTime)
{
    if (!entry.entryAnimation.has_value())
        return;

    Scene::EntryAnimation &entryAnimation{entry.entryAnimation.value()};
    std::vector<Animation> &animations{entryAnimation.skeletalAnimation.animations};

    if (index >= static_cast<int>(animations.size()))
        return;

    const Skeleton &skeleton{entryAnimation.skeletalAnimation.skeleton};

    entryAnimation.animator.play(animations[index], skeleton, entry.primitives, deltaTime);
}
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
#include "utilities/overloaded.h"
#include "utilities/texture_manager.h"
#include "utilities/transform.h"

namespace
{
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

    if (!skeletalAnimation)
        return {};

    auto &skin = skeletalAnimation->skeleton.skin;

    std::unordered_map<std::string, std::size_t> map;

    for (std::size_t i{0}; i < skeletalAnimation->animations.size(); ++i)
    {
        const auto &animation = skeletalAnimation->animations[i];

        if (!animation.name.empty())
            map.emplace(animation.name, i);
    }

    std::size_t numJoints{skin ? skin.value().invBindMatrices.size() : 0};

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

std::size_t Scene::findName(std::string_view name) const
{
    auto iter = entriesByName_.find(name.data());

    if (iter == entriesByName_.end() || iter->second >= entries_.size())
    {
        std::cerr << "ERROR: Cannot find named scene entry " << name << std::endl;
        throw std::runtime_error("Invalid key");
    }

    return iter->second;
}

const Scene::Entry &Scene::getEntry(std::string_view name) const
{
    return entries_[findName(name)];
}

const Scene::Entry &Scene::getEntry(Handle handle) const
{
    return entries_[findHandle(handle)];
}

Scene::Entry &Scene::editEntry(std::string_view name)
{
    std::size_t index{findName(name)};
    updatedModels_.insert(entries_[index].handle);

    return entries_[findName(name)];
}

Scene::Entry &Scene::editEntry(Handle handle)
{
    std::size_t index{findHandle(handle)};
    updatedModels_.insert(entries_[index].handle);

    return entries_[findHandle(handle)];
}

const DirectionalLight &Scene::getGlobalLight() const
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

const Camera &Scene::getCamera() const
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

const std::unordered_set<Handle> &Scene::getUpdatedHandles() const
{
    return updatedModels_;
}

Skeleton &Scene::getSkeleton(Scene::Entry &entry)
{
    if (!entry.entryAnimation)
        throw std::runtime_error("No animations");

    return entry.entryAnimation->skeletalAnimation.skeleton;
}

Animation &Scene::getAnimation(Scene::Entry &entry, std::string_view name)
{
    if (!entry.entryAnimation)
        throw std::runtime_error("No animations");

    std::unordered_map<std::string, std::size_t> &map{entry.entryAnimation->animationMap};

    auto nameIter = map.find(name.data());

    if (nameIter == map.end())
        throw std::runtime_error("Animation not found");

    Scene::EntryAnimation &entryAnimation{entry.entryAnimation.value()};
    return entryAnimation.skeletalAnimation.animations[nameIter->second];
}

Animation &Scene::getAnimation(Scene::Entry &entry, int index)
{
    if (!entry.entryAnimation)
        throw std::runtime_error("No animations");

    Scene::EntryAnimation &entryAnimation{entry.entryAnimation.value()};
    std::vector<Animation> &animations{entryAnimation.skeletalAnimation.animations};

    if (index >= static_cast<int>(animations.size()))
        throw std::runtime_error("Animation not found");

    return animations[index];
}

void Scene::clearUpdates()
{
    updatedModels_.clear();
    camera_.clearFlags();
    globalLightSource_.clearFlags();
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
                     const std::optional<Collision::Geometry> &collisionGeometry, const std::string &name)
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
        .collisionGeometry = collisionGeometry,
    });
}

void Scene::addCubemappedModel(Handle handle, const Model &model, const std::filesystem::path &imagePath,
                               const Transform &transform, ShaderType shaderType,
                               const std::optional<Collision::Geometry> &collisionGeometry, const std::string &name)
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
        .collisionGeometry = collisionGeometry,
    });
}

void Scene::addInstancedModel(Handle handle, const InstancedModel &model, ShaderType shaderType,
                              const std::optional<Collision::Geometry> &collisionGeometry, const std::string &name)
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
        .collisionGeometry = collisionGeometry,
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

    if (file.peek() == std::ifstream::traits_type::eof())
        return;

    SceneFile scene;

    try
    {
        scene = json::parse(file).get<SceneFile>();
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
    camera_.setAspectRatio(graphicsSettings.renderWidth / static_cast<float>(graphicsSettings.renderHeight));

    ambientLight_ = scene.environment.ambientLight;
    globalLightSource_ = DirectionalLight{scene.environment.lightColor, scene.environment.sunDirection};

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
                           addModel(handle, model, entry.transform, entry.shader, entry.collisionGeometry, entry.name);
                       },
                       [&](const InstancedModelEntry &entry) {
                           InstancedModel model{
                               entry.modelPath, entry.positionsPath,
                               InstancedModel::Randomize{.minScale = entry.minScale, .maxScale = entry.maxScale}};
                           addInstancedModel(handle, model, entry.shader, entry.collisionGeometry, entry.name);
                       },
                       [&](const CubeMappedModelEntry &entry) {
                           Model model{entry.modelPath};
                           addCubemappedModel(handle, model, entry.cubemapPath, entry.transform, entry.shader,
                                              entry.collisionGeometry, entry.name);
                       },
                   },
                   sceneEntry);

        validateSceneEntry(entries_.back());
        entriesByHandle_[handle] = entryIndex;
    }
}
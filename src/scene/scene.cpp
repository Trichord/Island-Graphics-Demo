#include "scene/scene.h"

#include <fastgltf/types.hpp>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <ios>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "models/instanced_model.h"
#include "models/model.h"
#include "models/particles.h"
#include "models/primitive.h"
#include "models/skybox.h"
#include "physics/collision_shapes.h"
#include "renderer/shader/graphics_shader.h"
#include "scene/camera.h"
#include "scene/directional_light.h"
#include "scene/entry_animation.h"
#include "scene/handle.h"
#include "scene/scene_file.h"
#include "utilities/overloaded.h"
#include "utilities/texture_manager.h"
#include "utilities/transform.h"

namespace
{
void validatePrimitives(Scene::Entry &entry)
{
    using enum ShaderType;
    using enum AlphaMode;

    if (entry.primitives.empty())
        throw std::runtime_error("Empty primitives");

    const bool isInstanced{entry.worldTransforms.size() > 1};

    for (Primitive &primitive : entry.primitives)
    {
        if (primitive.vertices.empty() || primitive.indices.empty())
            throw std::runtime_error("Empty vertices or indices");

        switch (primitive.shaderType)
        {
        case ocean:
            if (isInstanced)
                std::cerr << "WARNING: Multiple ocean instances sharing one reflection\n";

            if (primitive.alphaMode != opaque)
            {
                std::cerr << "WARNING: Overriding ocean shader primitive alpha mode with Opaque\n";
                primitive.alphaMode = opaque;
            }

            break;

        case water:
            if (isInstanced)
                std::cerr << "WARNING: Multiple water instances sharing one reflection\n";

            if (primitive.alphaMode != blend)
            {
                std::cerr << "WARNING: Overriding water shader primitive alpha mode with Blend\n";
                primitive.alphaMode = blend;
            }

            break;

        default:
            break;
        }
    }
}

void splitOnShaderType(Scene::Entry &entry)
{
    using enum ShaderType;
    using enum AlphaMode;

    for (Primitive &primitive : entry.primitives)
    {
        if (primitive.shaderType != trees)
            continue;

        if (primitive.alphaMode == opaque)
            primitive.shaderType = pbr;
        else
            primitive.shaderType = vegetation;
    }
}

void expandAnimatedAABBs(Scene::Entry &entry)
{
    constexpr float timeStep{0.1f};

    if (!entry.entryAnimation || !entry.entryAnimation->getNumJoints())
        return;

    for (std::size_t i{0}; i < entry.entryAnimation->getNumAnimations(); ++i)
    {
        entry.entryAnimation->setAnimation(i);
        const float animationLength{entry.entryAnimation->getAnimationLength(i)};

        for (float dt{0.0f}; dt <= animationLength; dt += timeStep)
        {
            entry.entryAnimation->updateAnimation(entry.primitives, timeStep);
            const std::vector<glm::mat4> &jointMatrices{entry.entryAnimation->getJointMatrices()};

            for (Primitive &primitive : entry.primitives)
            {
                if (!primitive.vertexSkins)
                    continue;

                glm::vec3 min{primitive.aabb.getMin()};
                glm::vec3 max{primitive.aabb.getMax()};
                const std::vector<Primitive::VertexSkin> &vertexSkins{primitive.vertexSkins.value()};

                for (std::size_t j{0}; j < primitive.vertices.size(); ++j)
                {
                    const glm::vec3 &position{primitive.vertices[j].position};
                    const glm::uvec4 &boneIDs{vertexSkins[j].boneIDs};
                    const glm::vec4 &weights{vertexSkins[j].weights};

                    glm::vec4 acc{0.0f};

                    for (glm::length_t k{0}; k < 4; ++k)
                    {
                        if (weights[k] > 0.0f)
                            acc += weights[k] * jointMatrices[boneIDs[k]] * glm::vec4(position, 1.0f);
                    }

                    min = glm::min(min, glm::vec3{acc});
                    max = glm::max(max, glm::vec3{acc});
                }

                primitive.aabb = {min, max};
            }
        }
    }

    entry.entryAnimation->setAnimation(0);
}

std::optional<EntryAnimation> createEntryAnimation(const Model &model)
{
    const fastgltf::Asset &asset{model.getAsset()};

    if (!asset.animations.size())
        return std::nullopt;

    return EntryAnimation{asset};
}

} // namespace

Scene::Scene(TextureManager &textureManager, const std::filesystem::path &jsonFile) : textureManager_{textureManager}
{
    setupScene(jsonFile);
}

const std::vector<Scene::Entry> &Scene::getAllSceneEntries() const
{
    return entries_;
}

std::size_t Scene::findIndex(Handle handle) const
{
    if (handle < 1 || handle > entries_.size() || entries_[handle - 1].handle != handle)
    {
        std::cerr << "Error: Could not find handle " << handle << "\n";
        throw std::runtime_error("Invalid handle");
    }

    return static_cast<std::size_t>(handle - 1);
}

std::size_t Scene::findIndex(std::string_view name) const
{
    auto iter = entriesByName_.find(name);

    if (iter == entriesByName_.end() || iter->second >= entries_.size())
    {
        std::cerr << "Error: Cannot find named scene entry " << name << "\n";
        throw std::runtime_error("Invalid key");
    }

    return iter->second;
}

const Scene::Entry &Scene::viewEntry(Handle handle) const
{
    return entries_[findIndex(handle)];
}

const Scene::Entry &Scene::viewEntry(std::string_view name) const
{
    return entries_[findIndex(name)];
}

Scene::Entry &Scene::editEntry(std::string_view name)
{
    std::size_t index{findIndex(name)};
    updatedEntries_.insert(entries_[index].handle);

    return entries_[index];
}

Scene::Entry &Scene::editEntry(Handle handle)
{
    std::size_t index{findIndex(handle)};
    updatedEntries_.insert(entries_[index].handle);

    return entries_[index];
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

const glm::vec3 &Scene::getWindDirection() const
{
    return windDirection_;
}

float Scene::getWindStrength() const
{
    return windStrength_;
}

const Camera::Description &Scene::getCameraDescription() const
{
    return cameraDescription_;
}

void Scene::clearUpdates()
{
    updatedEntries_.clear();
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

void Scene::addParticles(Handle handle, Particles &particles)
{
    textureManager_.addDefaultMaterial(handle);

    entries_.emplace_back(Scene::Entry{
        .handle = handle,
        .primitives = particles.takePrimitives(),
        .worldTransforms = {{}},
    });
}

void Scene::addSkybox(Handle handle, Skybox &skybox)
{
    const std::filesystem::path image{skybox.getPath()};

    textureManager_.addDefaultMaterial(handle);
    textureManager_.loadCubemapTextures(handle, image);

    entries_.emplace_back(Scene::Entry{
        .handle = handle,
        .primitives = skybox.takePrimitives(),
        .worldTransforms = {{}},
    });
}

void Scene::addModel(Handle handle, Model &model, const std::optional<collision::Geometry> &collisionGeometry,
                     const std::string &name, const std::optional<std::filesystem::path> &cubemapPath)
{
    if (!name.empty())
        entriesByName_.emplace(name, entries_.size());

    loadGLTFTextures(handle, model);

    if (cubemapPath && !cubemapPath->empty())
        textureManager_.loadCubemapTextures(handle, *cubemapPath);

    entries_.emplace_back(Scene::Entry{
        .handle = handle,
        .primitives = model.takePrimitives(),
        .entryAnimation = createEntryAnimation(model),
        .collisionGeometry = collisionGeometry,
    });
}

void Scene::setupScene(const std::filesystem::path &jsonFile)
{
    using namespace nlohmann;

    if (jsonFile.empty() || !std::filesystem::exists(jsonFile))
    {
        std::cerr << "Error: Could not load scene " << jsonFile << "\n";
        throw std::runtime_error("Invalid Scene");
    }

    std::ifstream file{jsonFile};

    if (!file.is_open())
    {
        std::cerr << "Error: Failed to open " << jsonFile << "\n";
        throw std::runtime_error("Failed to open file");
    }

    if (file.peek() == std::ifstream::traits_type::eof())
        return;

    SceneFile scene;

    try
    {
        json::parse(file).get_to(scene);
    }

    catch (const json::exception &e)
    {
        std::cerr << e.what() << "\n";
        throw;
    }

    cameraDescription_ = {.position = scene.camera.position,
                          .yaw = scene.camera.yaw,
                          .pitch = scene.camera.pitch,
                          .fov = scene.camera.fov,
                          .nearPlane = scene.camera.nearPlane,
                          .farPlane = scene.camera.farPlane};

    ambientLight_ = scene.environment.ambientLight;
    globalLightSource_ = DirectionalLight{scene.environment.lightColor, scene.environment.sunDirection};

    windStrength_ = scene.wind.strength;
    windDirection_ = scene.wind.direction;

    std::cout << "Loading Models... ";
    for (auto &fileEntry : scene.entries)
    {
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
                           if (entry.instancedEntry)
                           {
                               InstancedModel instanced{entry.modelPath, entry.shader,
                                                        entry.instancedEntry->positionsPath,
                                                        entry.instancedEntry->minScale, entry.instancedEntry->maxScale};
                               addModel(handle, instanced, entry.collisionGeometry, entry.name, entry.cubemapPath);
                               entries_.back().worldTransforms = std::move(instanced.loadInstancedTransforms());
                           }
                           else
                           {
                               Model model{entry.modelPath, entry.shader, entry.vertexExplosion};
                               addModel(handle, model, entry.collisionGeometry, entry.name, entry.cubemapPath);
                               entries_.back().worldTransforms = {std::get<Transform>(entry.position)};
                           }
                       },
                   },
                   fileEntry);
    }

    validateSceneEntries();
    std::cout << "Done\n";
}

void Scene::validateSceneEntries()
{
    for (Scene::Entry &entry : entries_)
    {
        validatePrimitives(entry);
        splitOnShaderType(entry);
        expandAnimatedAABBs(entry);
    }
}
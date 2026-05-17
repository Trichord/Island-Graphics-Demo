#include "scene.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "camera.h"
#include "directional_light.h"
#include "glm/glm.hpp"
#include "graphics_shader.h"
#include "handle.h"
#include "instanced_model.h"
#include "model.h"
#include "skybox.h"
#include "texture_manager.h"
#include "transform.h"

Scene::Scene(TextureManager &textureManager) : textureManager_{textureManager}
{
}

std::vector<SceneEntry> &Scene::getAllSceneEntries()
{
    return entries_;
}

SceneEntry &Scene::getEntryByName(const std::string &key)
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

    entries_.emplace_back(SceneEntry{
        .handle = handle,
        .aabb = skybox.getAABB(),
        .primitives = skybox.getPrimitives(),
        .transforms = {{}},
        .shaderType = ShaderType::skybox,
    });
}

void Scene::addModel(const Model &model, Transform &&transform, ShaderType shaderType,
                     const std::optional<std::string> name)
{
    const Handle handle{loadGLTFTextures(model)};

    if (name.has_value())
        entriesByName_.emplace(name.value(), entries_.size());

    entries_.emplace_back(SceneEntry{
        .handle = handle,
        .aabb = model.getAABB(),
        .primitives = model.getPrimitives(),
        .transforms = {std::move(transform)},
        .filePath = model.getPath(),
        .shaderType = shaderType,
    });
}

void Scene::addCubemappedModel(const Model &model, const std::filesystem::path &imagePath, Transform &&transform,
                               ShaderType shaderType, const std::optional<std::string> name)
{
    const Handle handle{loadGLTFTextures(model)};
    textureManager_.loadCubemapTextures(handle, imagePath);

    if (name.has_value())
        entriesByName_.emplace(name.value(), entries_.size());

    entries_.emplace_back(SceneEntry{
        .handle = handle,
        .aabb = model.getAABB(),
        .primitives = model.getPrimitives(),
        .transforms = {std::move(transform)},
        .filePath = model.getPath(),
        .shaderType = shaderType,
    });
}

void Scene::addInstancedModel(const InstancedModel &model, ShaderType shaderType, const std::optional<std::string> name)
{
    const Handle handle{loadGLTFTextures(model)};

    if (name.has_value())
        entriesByName_.emplace(name.value(), entries_.size());

    std::vector<Transform> transforms{model.loadInstancedTransforms()};

    entries_.emplace_back(SceneEntry{
        .handle = handle,
        .aabb = model.getAABB(),
        .primitives = model.getPrimitives(),
        .transforms = std::move(transforms),
        .filePath = model.getPath(),
        .shaderType = shaderType,
    });
}
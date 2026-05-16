#include "scene.h"

#include <functional>
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
#include "scene_resources.h"
#include "skybox.h"
#include "texture_manager.h"
#include "transform.h"

Scene::Scene(TextureManager &textureManager) : resources_{textureManager}
{
    entries_.reserve(sceneMaxObjects::maxTotal);
}

std::vector<SceneEntry> &Scene::getAllSceneEntries()
{
    return entries_;
}

SceneEntry &Scene::getEntryByName(const std::string &key)
{
    auto iter = entriesByName_.find(key);

    if (iter == entriesByName_.end())
    {
        std::cerr << "ERROR: Cannot find named scene entry " << key << std::endl;
        throw std::runtime_error("Invalid key");
    }

    return iter->second;
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

void Scene::addInstancedObjectToScene(InstancedModel &renderableRef, ShaderType shaderType,
                                      const std::optional<std::string> name)
{
    std::vector<Transform> transforms{renderableRef.loadInstancedTransforms()};

    // Store a reference to the stable vector in Scene Resources, organized by shader
    entries_.emplace_back(SceneEntry{
        .renderableRef = renderableRef,
        .transforms = std::move(transforms),
        .shaderType = shaderType,
    });

    // Store an access point to the instances, by model handle
    entriesByHandle_.emplace(renderableRef.getHandle(), std::ref(entries_.back()));

    // Add a lookup to get a reference to a model's instances by a user-defined name
    if (name.has_value())
        entriesByName_.emplace(name.value(), std::ref(entries_.back()));
}

void Scene::addSkyboxToScene(Skybox &skybox)
{
    std::vector<Transform> transforms{{}};

    entries_.emplace_back(SceneEntry{
        .renderableRef = skybox,
        .transforms = std::move(transforms),
        .shaderType = ShaderType::skybox,
    });
}
#include "scene.h"

#include <map>
#include <string>
#include <vector>

#include "camera.h"
#include "directional_light.h"
#include "glm/glm.hpp"
#include "light.h"
#include "point_light.h"
#include "renderable.h"
#include "shader_type.h"
#include "texture_manager.h"
#include "transform.h"

constexpr int maxModels{50};

Scene::Scene(TextureManager &textureManager) : sceneResources_{textureManager}
{
    sceneEntries_.reserve(maxModels);
}

std::vector<SceneEntry> &Scene::getAllSceneEntries()
{
    return sceneEntries_;
}

SceneEntry &Scene::getEntryByName(const std::string &key)
{
    auto iter = namedTransforms_.find(key);

    if (iter == namedTransforms_.end())
    {
        std::cerr << "ERROR: Cannot find named scene entry" << std::endl;
        throw std::runtime_error("Invalid key");
    }

    return iter->second;
}

DirectionalLight &Scene::getGlobalLightSource()
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

std::unordered_set<ModelHandle> &Scene::getTransformsUpdated()
{
    return updatedTransforms_;
}

SceneEntry &Scene::getEntryByHandle(ModelHandle handle)
{
    return transformsByHandle_.at(handle);
}
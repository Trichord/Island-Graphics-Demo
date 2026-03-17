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
    for (int i{0}; i < static_cast<int>(ShaderType::COUNT); ++i)
        sceneRenderables_[static_cast<ShaderType>(i)].reserve(maxModels);
}

std::map<ShaderType, std::vector<Renderable>> &Scene::getRenderablesByShader()
{
    return sceneRenderables_;
}

ModelInstances &Scene::getInstancesByName(const std::string &key)
{
    return namedInstances_.at(key);
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
    return transformsUpdated_;
}

std::vector<Instance> &Scene::getInstancesByHandle(ModelHandle handle) const
{
    return instancesByHandle_.at(handle);
}
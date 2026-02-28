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

Scene::Scene(TextureManager &textureManager) : sceneResources_{textureManager}
{
    using namespace sceneMaxObjects;

    pointLights_.reserve(maxLights);
    namedTransforms_.reserve(maxModels + maxLights);
}

std::map<ShaderType, std::vector<Renderable>> &Scene::getRenderablesByShader()
{
    return sceneRenderables_;
}

Transform &Scene::getObjectTransform(const std::string &key)
{
    return namedTransforms_.at(key).get();
}

Light &Scene::getNamedLight(const std::string &key)
{
    return namedLights_.at(key).get();
}

DirectionalLight &Scene::getGlobalLightSource()
{
    return globalLightSource_;
}

std::vector<PointLight> &Scene::getPointLights()
{
    return pointLights_;
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
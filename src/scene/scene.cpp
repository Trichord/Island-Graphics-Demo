#include "scene.h"

#include <map>
#include <string>
#include <vector>

#include "directional_light.h"
#include "glm/glm.hpp"
#include "light.h"
#include "point_light.h"
#include "renderable.h"
#include "shader_types.h"
#include "texture_manager.h"
#include "transform.h"

Scene::Scene(TextureManager& textureManager) : sceneResources{textureManager} {
  pointLights.reserve(sceneMaxObjects::maxLights);
}

std::map<ShaderType, std::vector<RenderableData>>& Scene::getSceneObjects() {
  return sceneObjects;
}

Transform& Scene::getObjectTransform(const std::string& key) {
  return namedTransforms.at(key).get();
}

Light& Scene::getNamedLight(const std::string& key) {
  return namedLights.at(key).get();
}

DirectionalLight& Scene::getGlobalLightSource() { return globalLightSource; }

std::vector<PointLight>& Scene::getPointLights() { return pointLights; }

glm::vec3 Scene::getAmbientLight() const { return ambientLight; }

void Scene::setAmbientLight(const glm::vec3& ambient) {
  ambientLight = ambient;
}
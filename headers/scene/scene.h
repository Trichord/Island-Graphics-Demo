#pragma once

#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "camera.h"
#include "config_window.h"
#include "directional_light.h"
#include "glm/glm.hpp"
#include "light.h"
#include "point_light.h"
#include "renderable.h"
#include "scene_resources.h"
#include "shader_types.h"
#include "transform.h"

/*
 * Any storage container (Models, lights, etc.) must guarantee that their
 * addresses remain valid for the lifetime of the scene
 *
 * Scene is for fast access of static objects - dynamic model loading requires
 * safer ownership model.
 */

namespace sceneMaxObjects
{
inline constexpr int maxLights{5};
} // namespace sceneMaxObjects

class Scene
{
  public:
    Scene(TextureManager &textureManager);
    Scene(const Scene &) = delete;
    Scene &operator=(const Scene &) = delete;
    Scene(Scene &&other) noexcept = default;
    Scene &operator=(Scene &&other) noexcept = default;
    virtual ~Scene() = default;

    Camera camera;

    glm::vec3 getAmbientLight() const;
    DirectionalLight &getGlobalLightSource();
    std::vector<PointLight> &getPointLights();
    Light &getNamedLight(const std::string &key);
    void setAmbientLight(const glm::vec3 &ambient);
    Transform &getObjectTransform(const std::string &key);
    std::map<ShaderType, std::vector<RenderableData>> &getSceneObjects();

  protected:
    glm::vec3 ambientLight{0.2f};
    SceneResources sceneResources;
    DirectionalLight globalLightSource;
    std::vector<PointLight> pointLights;
    std::map<ShaderType, std::vector<RenderableData>> sceneObjects;
    std::unordered_map<std::string, std::reference_wrapper<Light>> namedLights;
    std::unordered_map<std::string, std::reference_wrapper<Transform>> namedTransforms;

    virtual void addAllModelsToScene() = 0;
    virtual void addAllLightsToScene() = 0;
    virtual void setupCamera(const ConfigWindow::GraphicsSettings &graphicsSettings) = 0;

    virtual void initializeScene(const ConfigWindow::GraphicsSettings &graphicsSettings) = 0;

    /**
     * Adds an object to the scene.  Optional: give the object a name to reference
     * it in Application's onUpdate()
     * @param modelRef A reference to the model after its been loaded
     * using Scene's loading functions (e.g. loadModel, loadInstacedModel)
     * @param shaderType The shader this model will be rendered with
     * @param transform The position / rotation / scale transform applied to the
     * object
     */
    template <typename T>
    void addObjectToScene(T &modelRef, ShaderType shaderType, Transform &&transform = Transform{},
                          const std::optional<std::string> name = std::nullopt,
                          const std::optional<CullStrategy> overrideStrategy = std::nullopt)
    {
        CullStrategy strategy{overrideStrategy.value_or(getCullStrategy(shaderType))};

        sceneObjects[shaderType].emplace_back(RenderableData{std::ref(modelRef), transform, strategy});

        if (name.has_value())
            namedTransforms.emplace(name.value(), std::ref(sceneObjects[shaderType].back().transform));
    }

    /**
     * Adds a light to the scene.  Optional: name the light to reference it
     * in Application's onUpdate()
     * @param lightVec The vector to add the light to.
     * @param light The light object to insert.
     * @param name Optional name for later lookup.
     */
    template <typename LightType>
    void addLightToScene(std::vector<LightType> &lightVec, LightType &&light,
                         const std::optional<std::string> name = std::nullopt)
    {
        if (lightVec.size() >= sceneMaxObjects::maxLights)
            throw std::runtime_error("Max number of lights reached");

        lightVec.emplace_back(std::forward<LightType>(light));

        if (name.has_value())
            namedLights.emplace(name.value(), std::ref(lightVec.back()));
    }
};
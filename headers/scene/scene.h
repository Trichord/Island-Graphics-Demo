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
#include "shader_type.h"
#include "texture_manager.h"
#include "transform.h"

/*
 * Any storage container (Models, lights, etc.) must guarantee that their
 * addresses remain valid for the lifetime of the scene
 *
 * Scene is for fast access of static objects - dynamic model loading requires
 * safer ownership model.
 */

class Scene
{
  public:
    Scene(TextureManager &textureManager);
    Scene(const Scene &) = delete;
    Scene &operator=(const Scene &) = delete;
    Scene(Scene &&other) noexcept = default;
    Scene &operator=(Scene &&other) noexcept = default;
    virtual ~Scene() = default;

    Camera &getCamera();
    float getWindStrength() const;
    glm::vec3 getAmbientLight() const;
    const glm::vec3 &getWindDirection() const;
    DirectionalLight &getGlobalLightSource();
    void setAmbientLight(const glm::vec3 &ambient);
    Transform &getObjectTransform(const std::string &key);
    std::map<ShaderType, std::vector<Renderable>> &getRenderablesByShader();

  protected:
    Camera camera_;
    SceneResources sceneResources_;

    // Wind
    float windStrength_{0.0f};
    glm::vec3 windDirection_{0.0f};

    // Lights
    glm::vec3 ambientLight_{0.2f};
    DirectionalLight globalLightSource_;

    // Access points
    std::unordered_map<std::string, std::reference_wrapper<Transform>> namedTransforms_;
    std::map<ShaderType, std::vector<Renderable>> sceneRenderables_;

    virtual void addAllModelsToScene() = 0;
    virtual void setupEnvironment() = 0;
    virtual void onUpdate(float deltaTime) = 0;
    virtual void setupCamera(const ConfigWindow::GraphicsSettings &graphicsSettings) = 0;
    virtual void initializeScene(const ConfigWindow::GraphicsSettings &graphicsSettings) = 0;

    /**
     * @brief Adds an object to the scene.  Optional: give the object a name to reference
     * it in a scene's onUpdate() method.
     */
    template <typename T>
    void addObjectToScene(T &modelRef, ShaderType shaderType, ObjectProperties properties = {},
                          const std::optional<std::string> name = std::nullopt)
    {
        sceneRenderables_[shaderType].emplace_back(Renderable{std::ref(modelRef), std::move(properties)});

        if (name.has_value())
            namedTransforms_.emplace(
                name.value(), std::ref(std::get<Transform>(sceneRenderables_[shaderType].back().properties.transform)));
    }
};
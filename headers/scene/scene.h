#pragma once

#include <concepts>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "camera.h"
#include "directional_light.h"
#include "glm/glm.hpp"
#include "light.h"
#include "point_light.h"
#include "renderable.h"
#include "scene_resources.h"
#include "settings_window.h"
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

struct SceneEntry
{
    std::reference_wrapper<Renderable> renderableRef;
    std::vector<Instance> instances;

    CullMethod cullMethod{CullMethod::INSTANCED};
    ShaderType shaderType{ShaderType::DIRECTIONAL_PBR};
};

class Scene
{
  public:
    Scene(TextureManager &textureManager);
    Scene(const Scene &) = delete;
    Scene &operator=(const Scene &) = delete;
    Scene(Scene &&other) noexcept = default;
    Scene &operator=(Scene &&other) noexcept = default;
    virtual ~Scene() = default;

    // Environment
    float getWindStrength() const;
    glm::vec3 getAmbientLight() const;
    DirectionalLight &getGlobalLightSource();
    const glm::vec3 &getWindDirection() const;

    // Access to Scene objects
    Camera &getCamera();
    std::vector<SceneEntry> &getAllSceneEntries();
    SceneEntry &getEntryByName(const std::string &key);
    SceneEntry &getEntryByHandle(ModelHandle handle) const;

    // Per-frame Update
    std::unordered_set<ModelHandle> &getTransformsUpdated();

    // Mutators
    void setAmbientLight(const glm::vec3 &ambient);

  protected:
    struct ModelEntry
    {
        std::vector<Instance> instances;
        CullMethod cullMethod{CullMethod::INSTANCED};
    };

    Camera camera_;

    // Permanent storage
    SceneResources sceneResources_;

    // Associated data
    std::vector<SceneEntry> sceneEntries_;

    // Wind
    float windStrength_{0.0f};
    glm::vec3 windDirection_{0.0f};

    // Lights
    glm::vec3 ambientLight_{0.2f};
    DirectionalLight globalLightSource_;

    // Access points
    std::unordered_set<ModelHandle> transformsUpdated_;
    std::unordered_map<std::string, std::reference_wrapper<SceneEntry>> namedInstances_;
    std::unordered_map<ModelHandle, std::reference_wrapper<SceneEntry>> instancesByHandle_;

    virtual void addAllModelsToScene() = 0;
    virtual void setupEnvironment() = 0;
    virtual void onUpdate(float deltaTime) = 0;
    virtual void setupCamera(const GraphicsSettings &graphicsSettings) = 0;
    virtual void initializeScene(const GraphicsSettings &graphicsSettings) = 0;

    /**
     * @brief Adds an object to the scene.  Optional: give the object a name to access
     * it in a scene's onUpdate() method.
     */
    template <typename T>
        requires std::derived_from<T, Model>
    void addObjectToScene(T &modelRef, ShaderType shaderType, Transform &&transform = {},
                          CullMethod cullMethod = CullMethod::INSTANCED,
                          const std::optional<std::string> name = std::nullopt)
    {
        SceneEntry sceneEntry{.renderableRef = modelRef,
                              .instances = {{transform, modelRef.getAABB()}},
                              .cullMethod = cullMethod,
                              .shaderType = shaderType};

        // Store a reference to the stable vector in Scene Resources, organized by shader
        sceneEntries_.emplace_back(sceneEntry);

        // Store an access point to the instances, by model handle
        instancesByHandle_.emplace(modelRef.getHandle(), std::ref(sceneEntries_.back()));

        // Add a lookup to get a reference to a model's instances by a user-defined name
        if (name.has_value())
            namedInstances_.emplace(name.value(), std::ref(sceneEntries_.back()));
    }
};
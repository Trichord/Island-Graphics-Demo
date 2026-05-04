#pragma once

#include <concepts>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base_model.h"
#include "camera.h"
#include "directional_light.h"
#include "glm/glm.hpp"
#include "graphics_settings.h"
#include "instanced_model.h"
#include "model.h"
#include "scene_resources.h"
#include "graphics_shader.h"
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
    std::reference_wrapper<Model> modelRef;
    std::vector<Transform> transforms;

    ShaderType shaderType{ShaderType::directionalPBR};
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
    DirectionalLight &getGlobalLight();
    const glm::vec3 &getWindDirection() const;

    // Accessors
    Camera &getCamera();
    std::vector<SceneEntry> &getAllSceneEntries();
    SceneEntry &getEntryByName(const std::string &key);

    // Per-frame Update
    std::unordered_set<ModelHandle> &getUpdatedModels();

    // Mutators
    void setAmbientLight(const glm::vec3 &ambient);

  protected:
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
    std::unordered_set<ModelHandle> updatedModels_;
    std::unordered_map<std::string, std::reference_wrapper<SceneEntry>> entriesByName_;
    std::unordered_map<ModelHandle, std::reference_wrapper<SceneEntry>> entriesByHandle_;

    // For updating existing transforms. Do not add or remove them.
    virtual void onUpdate(float deltaTime) = 0;
    virtual void addAllModelsToScene() = 0;
    virtual void setupEnvironment() = 0;
    virtual void setupCamera(const GraphicsSettings &graphicsSettings) = 0;
    virtual void initializeScene(const GraphicsSettings &graphicsSettings) = 0;

    // Instanced models load positions and generate transforms from disk
    void addInstancedObjectToScene(InstancedModel &modelRef, ShaderType shaderType,
                                   const std::optional<std::string> name = std::nullopt);

    /**
     * @brief Adds an object to the scene.  Optional: give the object a name to access
     * it in a scene's onUpdate() method.
     */
    template <typename T>
        requires std::derived_from<T, BaseModel> && (!std::same_as<T, InstancedModel>)
    void addObjectToScene(T &modelRef, ShaderType shaderType, Transform &&transform = {},
                          const std::optional<std::string> name = std::nullopt)
    {
        SceneEntry sceneEntry{.modelRef = modelRef, .transforms = {transform}, .shaderType = shaderType};

        sceneEntries_.emplace_back(sceneEntry);

        // Add a lookup to get a reference to a model's instances by a user-defined name
        if (name.has_value())
            entriesByName_.emplace(name.value(), std::ref(sceneEntries_.back()));
    }
};
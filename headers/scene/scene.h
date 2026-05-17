#pragma once

#include <algorithm>
#include <concepts>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "aabb.h"
#include "camera.h"
#include "directional_light.h"
#include "glm/glm.hpp"
#include "graphics_settings.h"
#include "graphics_shader.h"
#include "handle.h"
#include "instanced_model.h"
#include "model.h"
#include "primitive.h"
#include "skybox.h"
#include "texture_manager.h"
#include "transform.h"

struct SceneEntry
{
    Handle handle{0};
    AABB aabb;

    std::vector<Primitive> primitives;
    std::vector<Transform> transforms;

    std::optional<std::filesystem::path> filePath;
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
    std::unordered_set<Handle> &getUpdatedModels();

    // Mutators
    void setAmbientLight(const glm::vec3 &ambient);

  protected:
    Camera camera_;
    TextureManager &textureManager_;

    // Associated data
    std::vector<SceneEntry> entries_;

    // Wind
    float windStrength_{0.0f};
    glm::vec3 windDirection_{0.0f};

    // Lights
    glm::vec3 ambientLight_{0.2f};
    DirectionalLight globalLightSource_;

    // Access points
    std::unordered_set<Handle> updatedModels_;
    std::unordered_map<std::string, std::size_t> entriesByName_;

    // For updating existing transforms. Do not add or remove them.
    virtual void onUpdate(float deltaTime) = 0;

    virtual void addAllModelsToScene() = 0;
    virtual void setupEnvironment() = 0;

    virtual void setupCamera(const GraphicsSettings &graphicsSettings) = 0;
    virtual void initializeScene(const GraphicsSettings &graphicsSettings) = 0;

    Handle loadGLTFTextures(const Model &model);

    void addSkybox(const Skybox &skybox);
    void addModel(const Model &model, Transform &&transform, ShaderType shaderType,
                  const std::optional<std::string> name = std::nullopt);
    void addInstancedModel(const InstancedModel &model, ShaderType shaderType,
                           const std::optional<std::string> name = std::nullopt);
    void addCubemappedModel(const Model &model, const std::filesystem::path &imagePath, Transform &&transform,
                            ShaderType shaderType, const std::optional<std::string> name = std::nullopt);
};
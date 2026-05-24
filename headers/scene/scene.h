#pragma once

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
#include "particles.h"
#include "primitive.h"
#include "skybox.h"
#include "texture_manager.h"
#include "transform.h"

class Scene
{
  public:
    struct Entry
    {
        Handle handle{0};
        AABB aabb;

        std::vector<Primitive> primitives;
        std::vector<Transform> transforms;

        std::optional<std::filesystem::path> filePath;
        ShaderType shaderType{ShaderType::directionalPBR};
    };

    Scene(TextureManager &_textureManager, const GraphicsSettings &graphicsSettings,
          const std::filesystem::path &jsonFile);
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
    std::vector<Entry> &getAllSceneEntries();
    Entry &getEntryByName(const std::string &key);
    Entry &getEntryByHandle(Handle handle);

    // Per-frame Update
    std::unordered_set<Handle> &getUpdatedModels();

    // Mutators
    void setAmbientLight(const glm::vec3 &ambient);

  protected:
    Camera camera_;
    TextureManager &textureManager_;

    // Associated data
    std::vector<Entry> entries_;

    // Wind
    float windStrength_{0.0f};
    glm::vec3 windDirection_{0.0f};

    // Lights
    glm::vec3 ambientLight_{0.2f};
    DirectionalLight globalLightSource_;

    // Access points
    std::unordered_set<Handle> updatedModels_;
    std::unordered_map<Handle, std::size_t> entriesByHandle_;
    std::unordered_map<std::string, std::size_t> entriesByName_;

    void setupScene(const std::filesystem::path &jsonFile, const GraphicsSettings &graphicsSettings);

    // For updating existing transforms. Do not add or remove them.
    virtual void onUpdate(float deltaTime) = 0;

    void loadGLTFTextures(Handle handle, const Model &model);

    void addSkybox(Handle handle, const Skybox &skybox);
    void addParticles(Handle handle, const Particles &particles);
    void addModel(Handle handle, const Model &model, const Transform &transform, ShaderType shaderType,
                  const std::string &name = {});
    void addInstancedModel(Handle handle, const InstancedModel &model, ShaderType shaderType,
                           const std::string &name = {});
    void addCubemappedModel(Handle handle, const Model &model, const std::filesystem::path &imagePath,
                            const Transform &transform, ShaderType shaderType, const std::string &name = {});
};
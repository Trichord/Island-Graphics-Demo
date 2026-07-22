#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "application/graphics_settings.h"
#include "models/animation/animator.h"
#include "models/animation/skeletal_animation.h"
#include "models/instanced_model.h"
#include "models/model.h"
#include "models/particles.h"
#include "models/primitive.h"
#include "models/skybox.h"
#include "renderer/shader/graphics_shader.h"
#include "scene/camera.h"
#include "scene/directional_light.h"
#include "scene/handle.h"
#include "utilities/texture_manager.h"
#include "utilities/transform.h"

class Scene
{
  public:
    struct EntryAnimation
    {
        SkeletalAnimation skeletalAnimation;
        Animator animator;
        std::unordered_map<std::string, std::size_t> animationMap;
    };

    struct Entry
    {
        Handle handle{0};
        ShaderType shaderType{ShaderType::directionalPBR};

        std::vector<Primitive> primitives;
        std::vector<Transform> worldTransforms;
        std::optional<EntryAnimation> entryAnimation;

        std::optional<std::filesystem::path> modelPath;
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
    const glm::vec3 &getAmbientLight() const;
    const DirectionalLight &getGlobalLight() const;
    const glm::vec3 &getWindDirection() const;

    // Accessors
    Camera &getCamera();
    const Camera &getCamera() const;
    const std::vector<Entry> &getAllSceneEntries() const;
    const Entry &getEntry(std::string_view name) const;
    const Entry &getEntry(Handle handle) const;

    // Per-frame Update
    const std::unordered_set<Handle> &getUpdatedHandles() const;
    void clearUpdates();

  protected:
    Camera camera_;

    virtual void onUpdate(float time, float deltaTime) = 0;

    [[nodiscard]] Entry &editEntry(Handle handle);
    [[nodiscard]] Entry &editEntry(std::string_view name);

    // Convenience methods
    Skeleton &getSkeleton(Scene::Entry &entry);
    Animation &getAnimation(Scene::Entry &entry, int index);
    Animation &getAnimation(Scene::Entry &entry, std::string_view name);

    void setAmbientLight(const glm::vec3 &ambient);

  private:
    TextureManager &textureManager_;
    Handle nextHandle_{1};

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

    void loadGLTFTextures(Handle handle, const Model &model);

    void addSkybox(Handle handle, const Skybox &skybox);
    void addParticles(Handle handle, const Particles &particles);
    void addModel(Handle handle, const Model &model, const Transform &transform, ShaderType shaderType,
                  const std::string &name = {});
    void addInstancedModel(Handle handle, const InstancedModel &model, ShaderType shaderType,
                           const std::string &name = {});
    void addCubemappedModel(Handle handle, const Model &model, const std::filesystem::path &imagePath,
                            const Transform &transform, ShaderType shaderType, const std::string &name = {});

    std::size_t findHandle(Handle handle) const;
    std::size_t findName(std::string_view name) const;
};
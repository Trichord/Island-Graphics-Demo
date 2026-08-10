#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "models/animation/animator.h"
#include "models/animation/skeletal_animation.h"
#include "models/instanced_model.h"
#include "models/model.h"
#include "models/particles.h"
#include "models/primitive.h"
#include "models/skybox.h"
#include "physics/collision_shapes.h"
#include "renderer/shader/graphics_shader.h"
#include "scene/camera.h"
#include "scene/directional_light.h"
#include "scene/handle.h"
#include "utilities/string_hash.h"
#include "utilities/texture_manager.h"
#include "utilities/transform.h"

class Scene
{
  public:
    struct EntryAnimation
    {
        SkeletalAnimation skeletalAnimation;
        Animator animator;
        std::unordered_map<std::string, std::size_t, StringHash, std::equal_to<>> animationMap;
    };

    struct Entry
    {
        Handle handle{0};
        ShaderType shaderType{ShaderType::directionalPBR};

        std::vector<Primitive> primitives;
        std::vector<Transform> worldTransforms;
        std::optional<EntryAnimation> entryAnimation;

        std::optional<Collision::Geometry> collisionGeometry;
    };

    Scene(TextureManager &_textureManager, const std::filesystem::path &jsonFile);
    Scene(const Scene &) = delete;
    Scene &operator=(const Scene &) = delete;
    Scene(Scene &&other) noexcept = delete;
    Scene &operator=(Scene &&other) noexcept = delete;
    virtual ~Scene() = default;

    // Environment
    float getWindStrength() const;
    const glm::vec3 &getAmbientLight() const;
    const DirectionalLight &getGlobalLight() const;
    const glm::vec3 &getWindDirection() const;
    const Camera::Description &getCameraDescription() const;

    // Accessors
    const std::vector<Entry> &getAllSceneEntries() const;
    const Entry &viewEntry(Handle handle) const;
    const Entry &viewEntry(std::string_view name) const;

    // Returns a non-contiguous view of updated scene entries, based on dirty handles. Return value must not outlive
    // Scene.
    auto getUpdatedEntries() const
    {
        return updatedEntries_ |
               std::views::transform([this](Handle handle) -> const Entry & { return entries_[findIndex(handle)]; });
    }

    virtual void onUpdate(const Camera &playerCamera, float time, float deltaTime) = 0;
    void clearUpdates();

  protected:
    [[nodiscard]] Entry &editEntry(Handle handle);
    [[nodiscard]] Entry &editEntry(std::string_view name);

    // Convenience methods
    Skeleton &getSkeleton(Scene::Entry &entry);
    Animation &getAnimation(Scene::Entry &entry, int index);
    Animation &getAnimation(Scene::Entry &entry, std::string_view name);

    void setAmbientLight(const glm::vec3 &ambient);

  private:
    TextureManager &textureManager_;

    // Handles must start at one. Zero is the sentinal "no handle"
    Handle nextHandle_{1};

    // Associated data
    Camera::Description cameraDescription_;
    std::vector<Entry> entries_;

    // Wind
    float windStrength_{0.0f};
    glm::vec3 windDirection_{0.0f};

    // Lights
    glm::vec3 ambientLight_{0.2f};
    DirectionalLight globalLightSource_;

    // Access points
    std::unordered_set<Handle> updatedEntries_;
    std::unordered_map<std::string, std::size_t, StringHash, std::equal_to<>> entriesByName_;

    void setupScene(const std::filesystem::path &jsonFile);

    void loadGLTFTextures(Handle handle, const Model &model);
    void addSkybox(Handle handle, const Skybox &skybox);
    void addParticles(Handle handle, const Particles &particles);
    void addModel(Handle handle, const Model &model, const Transform &transform, ShaderType shaderType,
                  const std::optional<Collision::Geometry> & = {}, const std::string &name = {});
    void addInstancedModel(Handle handle, const InstancedModel &model, ShaderType shaderType,
                           const std::optional<Collision::Geometry> &collisionGeometry = {},
                           const std::string &name = {});
    void addCubemappedModel(Handle handle, const Model &model, const std::filesystem::path &imagePath,
                            const Transform &transform, ShaderType shaderType,
                            const std::optional<Collision::Geometry> &collisionGeometry = {},
                            const std::string &name = {});

    std::size_t findIndex(Handle handle) const;
    std::size_t findIndex(std::string_view name) const;
};

using UpdatedEntriesView = decltype(std::declval<const Scene &>().getUpdatedEntries());
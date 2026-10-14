#pragma once

#include <filesystem>
#include <functional>
#include <glm/glm.hpp>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "game/camera.h"
#include "models/model.h"
#include "models/particles.h"
#include "models/primitive.h"
#include "models/skybox.h"
#include "physics/collision_shapes.h"
#include "physics/moved.h"
#include "physics/player_state.h"
#include "scene/directional_light.h"
#include "scene/entry_animation.h"
#include "scene/handle.h"
#include "utilities/string_hash.h"
#include "utilities/texture_manager.h"
#include "utilities/transform.h"

/**
 * @brief Owner of all scene objects in a unified format (Scene::Entry). Subclassed to provide gameplay logic through
 * virtual onUpdate method.
 */
class Scene
{
  public:
    struct Entry
    {
        Handle handle{0};

        std::vector<Primitive> primitives;
        std::vector<Transform> worldTransforms;
        std::optional<EntryAnimation> entryAnimation;
        std::optional<collision::Geometry> collisionGeometry;
    };

    Scene(TextureManager &_textureManager, const std::filesystem::path &jsonFile,
          const std::function<void(const std::string &)> &updateProgress);
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
    const std::vector<Scene::Entry> &getAllSceneEntries() const;
    const Scene::Entry &viewEntry(Handle handle) const;
    const Scene::Entry &viewEntry(std::string_view name) const;

    /**
     * @brief Gets all Scene entries that have been edited since the last call to clearUpdates()
     *
     * @return A const, non-contiguous view of updated scene entries, based on dirty handles
     */
    auto getUpdatedEntries() const
    {
        return updatedEntries_ | std::views::transform([this](Handle handle) -> const Scene::Entry & {
                   return entries_[findIndex(handle)];
               });
    }

    /**
     * @brief Updates all objects moved by the physics engine.
     */
    void physicsSync(const std::vector<MovedSceneObject> &movedObjects);

    /**
     * @brief This method is called once per frame, allowing subclasses to move scene objects and react to the player.
     */
    virtual void onUpdate(const PlayerState &player, float time, float deltaTime) = 0;
    void clear();

  protected:
    /**
     * @brief Gets a modifiable / updateable Scene object. Automatically flags the entry as modified. If no modification
     * is needed, use viewEntry.
     *
     * @param handle The unique Scene::Entry identifier.
     *
     * @return A reference to the entry.
     */
    [[nodiscard]] Scene::Entry &editEntry(Handle handle);

    /**
     * @brief Gets a modifiable / updateable Scene object. Automatically flags the entry as modified. If no modification
     * is needed, use viewEntry.
     *
     * @param name The Scene::Entry, as named by the Scene creator.
     *
     * @return A reference to the entry.
     */
    [[nodiscard]] Scene::Entry &editEntry(std::string_view name);

    void setAmbientLight(const glm::vec3 &ambient);

  private:
    TextureManager &textureManager_;

    // Handles must start at one. Zero is the sentinal "no handle"
    Handle nextHandle_{1};

    // Associated data
    Camera::Description cameraDescription_;
    std::vector<Scene::Entry> entries_;

    // Wind
    float windStrength_{0.0f};
    glm::vec3 windDirection_{0.0f};

    // Lights
    glm::vec3 ambientLight_{0.2f};
    DirectionalLight globalLightSource_;

    // Access points
    std::unordered_set<Handle> updatedEntries_;
    std::unordered_map<std::string, std::size_t, StringHash, std::equal_to<>> entriesByName_;

    void setupScene(const std::filesystem::path &jsonFile,
                    const std::function<void(const std::string &)> &updateProgress);
    void validateSceneEntries();

    void loadGLTFTextures(Handle handle, const Model &model);
    void addSkybox(Handle handle, Skybox &skybox);
    void addParticles(Handle handle, Particles &particles);
    void addModel(Handle handle, Model &model, const std::optional<collision::Geometry> &collisionGeometry = {},
                  const std::string &name = {}, const std::optional<std::filesystem::path> &cubemapPath = {});

    std::size_t findIndex(Handle handle) const;
    std::size_t findIndex(std::string_view name) const;
};

using UpdatedSceneEntries = decltype(std::declval<const Scene &>().getUpdatedEntries());
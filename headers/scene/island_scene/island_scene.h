#pragma once

#include <array>
#include <filesystem>
#include <functional>
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "application/graphics_settings.h"
#include "physics/player_state.h"
#include "scene/handle.h"
#include "scene/island_scene/seagull.h"
#include "scene/scene.h"
#include "utilities/texture_manager.h"

/**
 * @brief Concrete implementation of a Scene. An example of gameplay logic implemented through Scene's onUpdate method.
 */
class IslandScene : public Scene
{
  public:
    IslandScene(TextureManager &textureManager, const std::filesystem::path &jsonFile,
                const std::function<void(const std::string &)> &updateProgress);

    void onUpdate(const PlayerState &player, float time, float deltaTime) override;

  private:
    static constexpr int numSeagulls_{6};
    static constexpr std::array<std::string_view, numSeagulls_> seagullNames_{
        {"seagull1", "seagull2", "seagull3", "seagull4", "seagull5", "seagull6"}};
    static constexpr std::array<std::string_view, 3> seagullIdles_{{"eat", "look", "clearing"}};

    std::vector<glm::vec3> butterflyOriginalPos_;
    std::vector<Seagull> seagulls_ = std::vector<Seagull>(numSeagulls_);
    std::vector<Handle> dynamicObjects_;

    void updateSpaceModel(Scene::Entry &entry, float deltaTime);
    void updateButterfly(Scene::Entry &butterfly, float time, float deltaTime);
    void updateSeagull(const glm::vec3 &playerPos, Seagull &seagull, Scene::Entry &entry, float deltaTime);
};

#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <random>
#include <vector>

#include "application/graphics_settings.h"
#include "physics/player_state.h"
#include "scene/island_scene/seagull.h"
#include "scene/scene.h"
#include "utilities/texture_manager.h"

class IslandScene : public Scene
{
  public:
    IslandScene(TextureManager &textureManager, const std::filesystem::path &jsonFile);

    void onUpdate(const PlayerState &player, float time, float deltaTime) override;

  private:
    static constexpr int numSeagulls_{6};
    static constexpr std::array<std::string_view, numSeagulls_> seagullNames_{
        {"seagull1", "seagull2", "seagull3", "seagull4", "seagull5", "seagull6"}};

    std::vector<glm::vec3> butterflyOriginalPos_;
    std::vector<Seagull> seagulls_ = std::vector<Seagull>(numSeagulls_);

    Animation &randomSeagullIdle(Scene::Entry &entry);
    void updateSpaceModel(Scene::Entry &entry, float deltaTime);
    void updateButterfly(Scene::Entry &butterfly, float time, float deltaTime);
    void updateSeagull(const glm::vec3 &playerPosition, Seagull &seagull, Scene::Entry &entry, float deltaTime);
};

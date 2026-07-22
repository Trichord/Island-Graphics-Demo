#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <vector>

#include "application/graphics_settings.h"
#include "scene/island_scene/seagull.h"
#include "scene/scene.h"
#include "utilities/texture_manager.h"

class IslandScene : public Scene
{
  public:
    IslandScene(TextureManager &textureManager, const GraphicsSettings &graphicsSettings,
                const std::filesystem::path &jsonFile);

    void onUpdate(float time, float deltaTime) override;

  private:
    std::vector<glm::vec3> butterflyOriginalPos_;
    Seagull seagull_;
};

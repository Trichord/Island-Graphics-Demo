#pragma once

#include <filesystem>

#include "application/graphics_settings.h"
#include "scene/scene.h"
#include "utilities/texture_manager.h"

/* Concrete implementation of a scene */

class IslandScene : public Scene
{
  public:
    IslandScene(TextureManager &textureManager, const GraphicsSettings &graphicsSettings,
                const std::filesystem::path &jsonFile);

    void onUpdate(float deltaTime) override;
};

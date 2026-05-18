#pragma once

#include <filesystem>

#include "graphics_settings.h"
#include "scene.h"
#include "texture_manager.h"

/* Concrete implementation of a scene */

class IslandScene : public Scene
{
  public:
    IslandScene(TextureManager &textureManager, const GraphicsSettings &graphicsSettings,
                const std::filesystem::path &jsonFile);

    void onUpdate(float deltaTime) override;
};

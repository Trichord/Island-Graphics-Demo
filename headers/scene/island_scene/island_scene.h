#pragma once

#include "settings_window.h"
#include "scene.h"
#include "texture_manager.h"

/* Concrete implementation of a scene */

class IslandScene : public Scene
{
  public:
    IslandScene(TextureManager &_textureManager, const GraphicsSettings &graphicsSettings);

    void addAllModelsToScene() override;
    void setupEnvironment() override;
    void onUpdate(float deltaTime) override;
    void setupCamera(const GraphicsSettings &graphicsSettings) override;
    void initializeScene(const GraphicsSettings &graphicsSettings) override;
};

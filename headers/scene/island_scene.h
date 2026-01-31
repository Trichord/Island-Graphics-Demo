#pragma once

#include "config_window.h"
#include "scene.h"
#include "texture_manager.h"

/* Concrete implementation of a scene */

class IslandScene : public Scene
{
  public:
    IslandScene(TextureManager &_textureManager, const ConfigWindow::GraphicsSettings &graphicsSettings);

    void initializeScene(const ConfigWindow::GraphicsSettings &graphicsSettings) override;
    void addAllModelsToScene() override;
    void addAllLightsToScene() override;
    void setupCamera(const ConfigWindow::GraphicsSettings &graphicsSettings) override;
};

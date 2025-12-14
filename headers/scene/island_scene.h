#pragma once

#include "scene.h"
#include "texture_manager.h"

/* Concrete implementation of a scene */

class IslandScene : public Scene {
 public:
  IslandScene(TextureManager& textureManager);
  void addAllModelsToScene() override;
  void addAllLightsToScene() override;
  void setupCamera() override;
};

#pragma once

#include "scene.h"

/* Concrete implementation of a scene */

class IslandScene : public Scene {
 public:
  IslandScene();
  void addAllModelsToScene() override;
  void addAllLightsToScene() override;
  void setupCamera() override;
};

#pragma once

#include "scene.h"

/* Concrete implementation of a scene */

class DemoScene : public Scene {
 public:
  DemoScene();
  void addAllModelsToScene() override;
  void addAllLightsToScene() override;
  void setupCamera() override;
};

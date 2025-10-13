#pragma once

#include "scene.h"
#include "point_light.h"
#include "directional_light.h"


// Concrete implementation of a scene
class DemoScene : public Scene {
 public:
  DemoScene();
  void loadAllModels() override;
  void addLights() override;
};

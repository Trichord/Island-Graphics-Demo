#pragma once

#include <vector>

#include "model.h"
#include "renderable.h"
#include "scene.h"
#include "skybox.h"
#include "space_model.h"

// Concrete implementation of a scene
class DemoScene : public Scene {
 public:
  DemoScene();

 private:
  std::vector<Model> models;
  std::vector<SpaceModel> cubeMappedModels;
  std::vector<Skybox> skyboxes;

  void loadModels();
};

#pragma once


#include "model.h"

class Terrain : public Model {
 public:
  using Model::Model;
  Terrain(Terrain&& other) noexcept = default;

  bool perMeshCulling() const override;
};
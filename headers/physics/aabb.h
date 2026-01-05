#pragma once

class AABB {
 public:
  AABB(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);


 private:
  float minX{0.0f};
  float minY{0.0f};
  float minZ{0.0f};
  float maxX{0.0f};
  float maxY{0.0f};
  float maxZ{0.0f};
};
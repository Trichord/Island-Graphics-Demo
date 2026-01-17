#include "terrain.h"

bool Terrain::performCulling(const std::array<glm::vec4, 6>& frustumPlanes,
                             const glm::mat4& modelMatrix,
                             const glm::vec3& cameraPos,
                             const float cullDistanceScale) {
  bool visible{false};

  for (auto& mesh : meshes) {
    mesh.isVisible = false;
    for (const auto& primitive : mesh.primitives) {
      if (primitive.getAABB().intersects(frustumPlanes, modelMatrix)) {
        mesh.isVisible = true;
        visible = true;
        break;
      }
    }
  }

  return visible;
}
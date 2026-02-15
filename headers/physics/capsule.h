#include "glm/glm.hpp"

class Capsule
{
  public:
    Capsule() = default;
    Capsule(const glm::vec3 &position, float radius, float height);

    bool collision(const glm::vec3 &A, const glm::vec3 &B, const glm::vec3 &C);

  private:
    glm::vec3 top{1.0f};
    glm::vec3 bottom{0.0f};
    float radius{0.5};
};
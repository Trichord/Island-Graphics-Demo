#include "glm/glm.hpp"
#include <optional>

class Capsule
{
  public:
    Capsule() = default;
    Capsule(const glm::vec3 &position, float radius, float height);

    glm::vec3 getPosition() const;
    void setPosition(const glm::vec3 &newPosition);
    std::optional<glm::vec3> collision(const glm::vec3 &A, const glm::vec3 &B, const glm::vec3 &C);

  private:
    glm::vec3 top{1.0f};
    glm::vec3 bottom{0.0f};
    float radius{0.5};
    float height{1.0f};
};
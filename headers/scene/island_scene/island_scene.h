#pragma once

#include <filesystem>

#include "application/graphics_settings.h"
#include "scene/scene.h"
#include "utilities/texture_manager.h"

class IslandScene : public Scene
{
  public:
    IslandScene(TextureManager &textureManager, const GraphicsSettings &graphicsSettings,
                const std::filesystem::path &jsonFile);

    void onUpdate(float time, float deltaTime) override;

  private:
    // Update butterfly positions and animations
    static constexpr float butterflySpeed{5.0f};
    static constexpr float heightMultiplier{0.04f};
    static constexpr float bobSpeed{79.6227f};
    static constexpr float radius{8.0f};
    static constexpr float largeHeightWave{0.65f};

    // Rotate the fancy space model
    static constexpr float degreesPerSecond{2.0f};
    static constexpr glm::vec3 oddAxisAngle{0.2926f, 0.8048f, 0.5164f};
    static constexpr glm::vec3 oddAxisAngle2{-0.3197f, 0.7975f, 0.5117f};

    std::vector<glm::vec3> butterflyOriginalPos_;
};

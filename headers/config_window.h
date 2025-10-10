#pragma once

struct GraphicsSettings {
  unsigned int screenWidth;
  unsigned int screenHeight;
  bool fullscreen;
  bool vsync;
  unsigned int msaa;
  float anisotropicFiltering;
};

class ConfigWindow {
 public:
  ConfigWindow() = default;
  bool createSettingsWindow();
  const GraphicsSettings& getGraphicsSettings() const;

 private:
  GraphicsSettings graphicsSettings = {1920, 1080, true, false, 0, 0.0f};
};
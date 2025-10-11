#pragma once
#include "glad/glad.h"

struct GraphicsSettings {
  unsigned int screenWidth;
  unsigned int screenHeight;
  bool fullscreen;
  bool vsync;
  unsigned int msaa;
  float anisotropicFiltering;
  float gamma;
};

class ConfigWindow {
 public:
  ConfigWindow() = default;
  ConfigWindow(const ConfigWindow&) = delete;
  ConfigWindow& operator=(const ConfigWindow&) = delete;
  ConfigWindow(ConfigWindow&& other) = delete;
  ConfigWindow& operator=(ConfigWindow&& other) = delete;
  ~ConfigWindow();

  bool createSettingsWindow();
  const GraphicsSettings& getGraphicsSettings() const;

 private:
  GraphicsSettings graphicsSettings = {1920, 1080, true, false, 0, 0.0f, 1.0f};
  inline static const char* resolutions[] = {"3840 x 2160", "2560 x 1440",
                                      "1920 x 1080", "1600 x 900",
                                      "1366 x 768",  "1280 x 720"};

  inline static const char* msaaLevel[] = {"Off", "2x", "4x", "8x"};
  inline static const char* anisoLevel[] = {"Off", "2x", "4x", "8x", "16x"};

  inline static int resolution_selected_idx = 1;
  inline static int msaa_selected_idx = 0;
  inline static int aniso_selected_idx = 0;
  inline static const char* resolution_preview_value =
      resolutions[resolution_selected_idx];
  inline static const char* msaa_preview_value = msaaLevel[msaa_selected_idx];
  inline static const char* aniso_preview_value =
      anisoLevel[aniso_selected_idx];

  GLuint textureID = 0;

  void loadConfigWindowImage();
};
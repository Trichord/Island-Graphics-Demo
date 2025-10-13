#pragma once

#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include <utility>
#include <array>

class ConfigWindow {
 public:
  static inline struct GraphicsSettings {
    unsigned int screenWidth = 2560;
    unsigned int screenHeight = 1440;
    bool fullscreen = true;
    bool vsync = false;
    unsigned int msaa = 8;
    float anisotropicFiltering = 16.0f;
    float gamma = 1.0f;
  } globalGraphicsSettings;

  ConfigWindow() = default;
  ConfigWindow(const ConfigWindow&) = delete;
  ConfigWindow& operator=(const ConfigWindow&) = delete;
  ConfigWindow(ConfigWindow&& other) = delete;
  ConfigWindow& operator=(ConfigWindow&& other) = delete;
  ~ConfigWindow();

  bool launchSettingsWindow();

 private:
  GLFWwindow* window = nullptr;
  inline static const std::array<const char*, 6> resolutions = {
      "3840 x 2160", "2560 x 1440", "1920 x 1080",
      "1600 x 900",  "1366 x 768",  "1280 x 720"};

  inline static const std::array<std::pair<unsigned int, unsigned int>, 6>
      resolutionTable = {std::pair(3840, 2160), std::pair(2560, 1440),
                         std::pair(1920, 1080), std::pair(1600, 900),
                         std::pair(1366, 768),  std::pair(1280, 720)};

  inline static const std::array<const char*, 4> msaaLevel = {"Off", "2x", "4x",
                                                              "8x"};
  inline static const std::array<unsigned int, 4> msaaTable = {0, 2, 4, 8};

  inline static const std::array<const char*, 5> anisoLevel = {
      "Off", "2x", "4x", "8x", "16x"};
  inline static const std::array<float, 5> anisoTable = {0.0f, 2.0f, 4.0f, 8.0f,
                                                         16.0f};

  inline static int resolution_selected_idx = 1;
  inline static int msaa_selected_idx = 3;
  inline static int aniso_selected_idx = 4;
  inline static const char* resolution_preview_value =
      resolutions[resolution_selected_idx];
  inline static const char* msaa_preview_value = msaaLevel[msaa_selected_idx];
  inline static const char* aniso_preview_value =
      anisoLevel[aniso_selected_idx];

  GLuint textureID = 0;

  bool createWindow();
  void setupImGui();
  void loadConfigWindowImage();
  bool renderImGui();

  void updateGlobalResolution(unsigned int index);
  void updateGlobalMSAA(unsigned int index);
  void updateGlobalAniso(unsigned int index);
};